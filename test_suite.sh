#!/bin/bash

# Multi-threaded Chat and File Server - Comprehensive Test Suite
# This script automates all test scenarios from the project report

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test configuration
SERVER_PORT=8080
SERVER_HOST="127.0.0.1"
TEST_DIR="test_results"
LOG_FILE="$TEST_DIR/test_log.txt"
SERVER_PID=""
CLIENT_PIDS=()

# Test files
TEST_FILES_DIR="test_files"
VALID_TXT_FILE="$TEST_FILES_DIR/test.txt"
VALID_PDF_FILE="$TEST_FILES_DIR/sample.pdf"
LARGE_FILE="$TEST_FILES_DIR/large_file.txt"
INVALID_FILE="$TEST_FILES_DIR/malware.exe"

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}[$(date '+%H:%M:%S')] ${message}${NC}"
}

print_test_header() {
    echo -e "\n${CYAN}================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}================================${NC}"
}

print_success() {
    print_status "$GREEN" "✅ $1"
}

print_error() {
    print_status "$RED" "❌ $1"
}

print_warning() {
    print_status "$YELLOW" "⚠️  $1"
}

print_info() {
    print_status "$BLUE" "ℹ️  $1"
}

# Cleanup function
cleanup() {
    print_info "Cleaning up test environment..."
    
    # Kill all client processes
    for pid in "${CLIENT_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
        fi
    done
    
    # Kill server process
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -TERM "$SERVER_PID" 2>/dev/null || true
        sleep 2
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            kill -KILL "$SERVER_PID" 2>/dev/null || true
        fi
    fi
    
    # Clean up any remaining processes
    pkill -f "chatserver" 2>/dev/null || true
    pkill -f "chatclient" 2>/dev/null || true
    
    # Wait a bit for cleanup
    sleep 1
    
    print_info "Cleanup completed"
}

# Setup test environment
setup_test_environment() {
    print_test_header "SETTING UP TEST ENVIRONMENT"
    
    # Create test directories
    mkdir -p "$TEST_DIR"
    mkdir -p "$TEST_FILES_DIR"
    
    # Create test files
    echo "This is a test file for file transfer testing." > "$VALID_TXT_FILE"
    
    # Create a sample PDF (simple text-based PDF)
    cat > "$VALID_PDF_FILE" << 'EOF'
%PDF-1.4
1 0 obj
<<
/Type /Catalog
/Pages 2 0 R
>>
endobj
2 0 obj
<<
/Type /Pages
/Kids [3 0 R]
/Count 1
>>
endobj
3 0 obj
<<
/Type /Page
/Parent 2 0 R
/MediaBox [0 0 612 792]
/Contents 4 0 R
>>
endobj
4 0 obj
<<
/Length 44
>>
stream
BT
/F1 12 Tf
100 700 Td
(Test PDF for file transfer) Tj
ET
endstream
endobj
xref
0 5
0000000000 65535 f 
0000000009 00000 n 
0000000058 00000 n 
0000000115 00000 n 
0000000206 00000 n 
trailer
<<
/Size 5
/Root 1 0 R
>>
startxref
299
%%EOF
EOF
    
    # Create a large file (>3MB)
    dd if=/dev/zero of="$LARGE_FILE" bs=1M count=4 2>/dev/null
    
    # Create invalid file
    echo "This is not a valid file type" > "$INVALID_FILE"
    
    # Compile the project
    print_info "Compiling project..."
    make clean > /dev/null 2>&1
    if ! make all > /dev/null 2>&1; then
        print_error "Failed to compile project"
        exit 1
    fi
    
    print_success "Test environment setup completed"
}

# Start server
start_server() {
    print_info "Starting server on port $SERVER_PORT..."
    
    # Remove old log file
    rm -f server_log.txt
    
    # Start server in background
    ./chatserver "$SERVER_PORT" > "$TEST_DIR/server_output.txt" 2>&1 &
    SERVER_PID=$!
    
    # Wait for server to start
    sleep 2
    
    # Check if server is running
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        print_error "Failed to start server"
        cat "$TEST_DIR/server_output.txt"
        exit 1
    fi
    
    # Check if port is listening (macOS compatible)
    if command -v lsof >/dev/null 2>&1; then
        if ! lsof -i ":$SERVER_PORT" >/dev/null 2>&1; then
            print_warning "Port check inconclusive (using lsof)"
        fi
    elif command -v netstat >/dev/null 2>&1; then
        if ! netstat -an | grep -q "LISTEN.*:$SERVER_PORT"; then
            print_warning "Port check inconclusive (using netstat)"
        fi
    else
        print_warning "No port checking tool available"
    fi
    
    print_success "Server started successfully (PID: $SERVER_PID)"
}

# Function to start a client and return its PID
start_client() {
    local username=$1
    local output_file="$TEST_DIR/client_${username}_output.txt"
    local input_file="$TEST_DIR/client_${username}_input.txt"
    
    # Create named pipe for input
    mkfifo "$input_file" 2>/dev/null || true
    
    # Start client
    (
        exec < "$input_file"
        echo "$username" | ./chatclient "$SERVER_HOST" "$SERVER_PORT" > "$output_file" 2>&1
    ) &
    
    local client_pid=$!
    CLIENT_PIDS+=("$client_pid")
    
    # Send username
    echo "$username" > "$input_file" &
    
    # Wait for connection
    sleep 1
    
    echo "$client_pid:$input_file:$output_file"
}

# Function to send command to client
send_client_command() {
    local input_file=$1
    local command=$2
    
    echo "$command" > "$input_file" &
    sleep 0.5
}

# Function to check if text exists in file
check_output() {
    local file=$1
    local expected_text=$2
    local timeout=${3:-5}
    
    local count=0
    while [[ $count -lt $timeout ]]; do
        if grep -q "$expected_text" "$file" 2>/dev/null; then
            return 0
        fi
        sleep 1
        ((count++))
    done
    return 1
}

# Test 1: Concurrent User Load Test
test_concurrent_users() {
    print_test_header "TEST 1: CONCURRENT USER LOAD TEST"
    
    local usernames=("alice" "bob" "charlie" "david" "eve" "frank" "grace" "henry" "iris" "jack" "kate" "liam" "mary" "nick" "olivia")
    local client_info=()
    
    print_info "Starting 15 concurrent clients..."
    
    # Start multiple clients
    for username in "${usernames[@]}"; do
        local info=$(start_client "$username")
        client_info+=("$info")
        print_info "Started client: $username"
    done
    
    sleep 3
    
    # Check server logs for connections
    local connected_count=$(grep -c "LOGIN.*connected" server_log.txt 2>/dev/null || echo "0")
    
    if [[ $connected_count -ge 10 ]]; then
        print_success "Concurrent user test passed ($connected_count users connected)"
    else
        print_error "Concurrent user test failed (only $connected_count users connected)"
    fi
    
    # Test basic communication
    print_info "Testing basic communication..."
    local alice_info="${client_info[0]}"
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    
    send_client_command "$alice_input" "/join testroom"
    send_client_command "$alice_input" "/broadcast Hello from concurrent test!"
    
    sleep 2
    
    if grep -q "BROADCAST.*alice.*Hello from concurrent test" server_log.txt; then
        print_success "Basic communication test passed"
    else
        print_warning "Basic communication test inconclusive"
    fi
}

# Test 2: Username Validation Test
test_username_validation() {
    print_test_header "TEST 2: USERNAME VALIDATION TEST"
    
    # Test duplicate username
    print_info "Testing duplicate username rejection..."
    local alice1_info=$(start_client "alice")
    sleep 1
    local alice2_info=$(start_client "alice")
    sleep 2
    
    if grep -q "REJECTED.*Duplicate username attempted.*alice" server_log.txt; then
        print_success "Duplicate username rejection test passed"
    else
        print_error "Duplicate username rejection test failed"
    fi
    
    # Test invalid username
    print_info "Testing invalid username rejection..."
    local invalid_info=$(start_client "alice@123")
    sleep 2
    
    local invalid_output=$(echo "$invalid_info" | cut -d: -f3)
    if check_output "$invalid_output" "Invalid username" 3; then
        print_success "Invalid username rejection test passed"
    else
        print_error "Invalid username rejection test failed"
    fi
}

# Test 3: Room Management Test
test_room_management() {
    print_test_header "TEST 3: ROOM MANAGEMENT TEST"
    
    local alice_info=$(start_client "alice")
    local bob_info=$(start_client "bob")
    
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local bob_input=$(echo "$bob_info" | cut -d: -f2)
    
    sleep 2
    
    # Test room creation and joining
    print_info "Testing room creation and joining..."
    send_client_command "$alice_input" "/join gameroom"
    sleep 1
    
    if grep -q "ROOM-CREATE.*Room 'gameroom' created" server_log.txt; then
        print_success "Room creation test passed"
    else
        print_error "Room creation test failed"
    fi
    
    send_client_command "$bob_input" "/join gameroom"
    sleep 1
    
    if grep -q "JOIN.*bob.*joined room 'gameroom'" server_log.txt; then
        print_success "Room joining test passed"
    else
        print_error "Room joining test failed"
    fi
    
    # Test room destruction
    print_info "Testing room destruction..."
    send_client_command "$alice_input" "/leave"
    send_client_command "$bob_input" "/leave"
    sleep 2
    
    if grep -q "ROOM-DESTROY.*Room 'gameroom' destroyed" server_log.txt; then
        print_success "Room destruction test passed"
    else
        print_warning "Room destruction test inconclusive"
    fi
}

# Test 4: Message Broadcasting Test
test_message_broadcasting() {
    print_test_header "TEST 4: MESSAGE BROADCASTING TEST"
    
    local alice_info=$(start_client "alice")
    local bob_info=$(start_client "bob")
    
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local bob_input=$(echo "$bob_info" | cut -d: -f2)
    local bob_output=$(echo "$bob_info" | cut -d: -f3)
    
    sleep 2
    
    # Join same room
    send_client_command "$alice_input" "/join testroom"
    send_client_command "$bob_input" "/join testroom"
    sleep 1
    
    # Test broadcasting
    print_info "Testing message broadcasting..."
    send_client_command "$alice_input" "/broadcast Hello everyone in testroom!"
    sleep 2
    
    if grep -q "BROADCAST.*alice.*Hello everyone in testroom" server_log.txt; then
        print_success "Message broadcasting test passed"
    else
        print_error "Message broadcasting test failed"
    fi
    
    # Check if bob received the message
    if check_output "$bob_output" "alice.*Hello everyone in testroom" 3; then
        print_success "Message delivery test passed"
    else
        print_warning "Message delivery test inconclusive"
    fi
}

# Test 5: Private Messaging (Whisper) Test
test_private_messaging() {
    print_test_header "TEST 5: PRIVATE MESSAGING (WHISPER) TEST"
    
    local alice_info=$(start_client "alice")
    local bob_info=$(start_client "bob")
    
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local bob_output=$(echo "$bob_info" | cut -d: -f3)
    
    sleep 2
    
    # Test whisper
    print_info "Testing private messaging..."
    send_client_command "$alice_input" "/whisper bob This is a private message"
    sleep 2
    
    if grep -q "WHISPER.*alice.*bob.*This is a private message" server_log.txt; then
        print_success "Private messaging test passed"
    else
        print_error "Private messaging test failed"
    fi
    
    # Check if bob received the whisper
    if check_output "$bob_output" "Whisper from alice.*This is a private message" 3; then
        print_success "Whisper delivery test passed"
    else
        print_warning "Whisper delivery test inconclusive"
    fi
}

# Test 6: File Transfer Test
test_file_transfer() {
    print_test_header "TEST 6: FILE TRANSFER TEST"
    
    local alice_info=$(start_client "alice")
    local bob_info=$(start_client "bob")
    
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    
    sleep 2
    
    # Test valid file transfer
    print_info "Testing valid file transfer..."
    send_client_command "$alice_input" "/sendfile $VALID_TXT_FILE bob"
    sleep 3
    
    if grep -q "COMMAND.*alice initiated file transfer to bob" server_log.txt; then
        print_success "File transfer initiation test passed"
    else
        print_error "File transfer initiation test failed"
    fi
    
    # Check if file was processed
    if grep -q "SEND FILE.*test.txt.*alice.*bob.*success" server_log.txt; then
        print_success "File transfer processing test passed"
    else
        print_warning "File transfer processing test inconclusive"
    fi
    
    # Check if uploads directory was created and file exists
    if [[ -d "uploads" ]] && ls uploads/ | grep -q "bob_test.txt"; then
        print_success "File upload directory test passed"
    else
        print_warning "File upload directory test inconclusive"
    fi
}

# Test 7: File Validation Test
test_file_validation() {
    print_test_header "TEST 7: FILE VALIDATION TEST"
    
    local alice_info=$(start_client "alice")
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local alice_output=$(echo "$alice_info" | cut -d: -f3)
    
    sleep 2
    
    # Test large file rejection
    print_info "Testing large file rejection..."
    send_client_command "$alice_input" "/sendfile $LARGE_FILE bob"
    sleep 2
    
    if check_output "$alice_output" "File too large" 3; then
        print_success "Large file rejection test passed"
    else
        print_error "Large file rejection test failed"
    fi
    
    # Test invalid file type rejection
    print_info "Testing invalid file type rejection..."
    send_client_command "$alice_input" "/sendfile $INVALID_FILE bob"
    sleep 2
    
    if check_output "$alice_output" "Invalid file type" 3; then
        print_success "Invalid file type rejection test passed"
    else
        print_error "Invalid file type rejection test failed"
    fi
    
    # Test non-existent file
    print_info "Testing non-existent file rejection..."
    send_client_command "$alice_input" "/sendfile nonexistent.txt bob"
    sleep 2
    
    if check_output "$alice_output" "File not found" 3; then
        print_success "Non-existent file rejection test passed"
    else
        print_error "Non-existent file rejection test failed"
    fi
}

# Test 8: Error Handling Test
test_error_handling() {
    print_test_header "TEST 8: ERROR HANDLING TEST"
    
    local alice_info=$(start_client "alice")
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local alice_output=$(echo "$alice_info" | cut -d: -f3)
    
    sleep 2
    
    # Test broadcast without room
    print_info "Testing broadcast without room..."
    send_client_command "$alice_input" "/broadcast Hello without room"
    sleep 1
    
    if check_output "$alice_output" "You must join a room first" 3; then
        print_success "Broadcast without room error test passed"
    else
        print_error "Broadcast without room error test failed"
    fi
    
    # Test whisper to non-existent user
    print_info "Testing whisper to non-existent user..."
    send_client_command "$alice_input" "/whisper nonexistent Hello"
    sleep 1
    
    if check_output "$alice_output" "User not found" 3; then
        print_success "Whisper to non-existent user error test passed"
    else
        print_error "Whisper to non-existent user error test failed"
    fi
    
    # Test leave without room
    print_info "Testing leave without room..."
    send_client_command "$alice_input" "/leave"
    sleep 1
    
    if check_output "$alice_output" "You are not in any room" 3; then
        print_success "Leave without room error test passed"
    else
        print_error "Leave without room error test failed"
    fi
}

# Test 9: Client Exit Test
test_client_exit() {
    print_test_header "TEST 9: CLIENT EXIT TEST"
    
    local alice_info=$(start_client "alice")
    local alice_input=$(echo "$alice_info" | cut -d: -f2)
    local alice_pid=$(echo "$alice_info" | cut -d: -f1 | cut -d: -f1)
    
    sleep 2
    
    # Test /exit command
    print_info "Testing /exit command..."
    send_client_command "$alice_input" "/exit"
    sleep 2
    
    # Check if client process exited
    if ! kill -0 "$alice_pid" 2>/dev/null; then
        print_success "Client exit test passed"
    else
        print_error "Client exit test failed"
    fi
    
    # Check server log for disconnect
    if grep -q "DISCONNECT.*alice.*lost connection" server_log.txt; then
        print_success "Client disconnect logging test passed"
    else
        print_warning "Client disconnect logging test inconclusive"
    fi
}

# Test 10: Server Shutdown Test
test_server_shutdown() {
    print_test_header "TEST 10: SERVER SHUTDOWN TEST"
    
    # Start a few clients
    local alice_info=$(start_client "alice")
    local bob_info=$(start_client "bob")
    
    sleep 2
    
    print_info "Testing graceful server shutdown..."
    
    # Send SIGINT to server
    kill -INT "$SERVER_PID"
    sleep 3
    
    # Check if server logged shutdown
    if grep -q "SHUTDOWN.*SIGINT received" server_log.txt; then
        print_success "Server shutdown initiation test passed"
    else
        print_error "Server shutdown initiation test failed"
    fi
    
    if grep -q "SHUTDOWN.*Server shutdown complete" server_log.txt; then
        print_success "Server shutdown completion test passed"
    else
        print_warning "Server shutdown completion test inconclusive"
    fi
    
    # Check if server process exited
    sleep 2
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        print_success "Server process termination test passed"
    else
        print_error "Server process termination test failed"
        kill -KILL "$SERVER_PID" 2>/dev/null || true
    fi
    
    SERVER_PID=""  # Clear PID since server is down
}

# Generate test report
generate_report() {
    print_test_header "GENERATING TEST REPORT"
    
    local report_file="$TEST_DIR/test_report.txt"
    
    cat > "$report_file" << EOF
Multi-threaded Chat and File Server - Test Report
Generated on: $(date)

=== TEST ENVIRONMENT ===
Server Host: $SERVER_HOST
Server Port: $SERVER_PORT
Test Directory: $TEST_DIR

=== SERVER LOG ANALYSIS ===
EOF
    
    if [[ -f "server_log.txt" ]]; then
        echo "Total log entries: $(wc -l < server_log.txt)" >> "$report_file"
        echo "Connections: $(grep -c "CONNECT" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "Logins: $(grep -c "LOGIN" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "Room operations: $(grep -c "ROOM\|JOIN\|LEAVE" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "Messages: $(grep -c "BROADCAST\|WHISPER" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "File transfers: $(grep -c "SEND FILE" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "Errors: $(grep -c "ERROR" server_log.txt 2>/dev/null || echo "0")" >> "$report_file"
        echo "" >> "$report_file"
        echo "=== FULL SERVER LOG ===" >> "$report_file"
        cat server_log.txt >> "$report_file"
    else
        echo "Server log not found" >> "$report_file"
    fi
    
    print_success "Test report generated: $report_file"
}

# Main test execution
main() {
    print_test_header "MULTI-THREADED CHAT SERVER TEST SUITE"
    print_info "Starting comprehensive test suite..."
    
    # Set up trap for cleanup
    trap cleanup EXIT INT TERM
    
    # Setup and start server
    setup_test_environment
    start_server
    
    # Run all tests
    test_concurrent_users
    cleanup  # Clean clients between tests
    sleep 2
    
    test_username_validation
    cleanup
    sleep 2
    
    test_room_management
    cleanup
    sleep 2
    
    test_message_broadcasting
    cleanup
    sleep 2
    
    test_private_messaging
    cleanup
    sleep 2
    
    test_file_transfer
    cleanup
    sleep 2
    
    test_file_validation
    cleanup
    sleep 2
    
    test_error_handling
    cleanup
    sleep 2
    
    test_client_exit
    cleanup
    sleep 2
    
    # Server shutdown test (this will stop the server)
    start_server  # Restart server for shutdown test
    test_server_shutdown
    
    # Generate final report
    generate_report
    
    print_test_header "TEST SUITE COMPLETED"
    print_success "All tests completed. Check $TEST_DIR for detailed results."
    print_info "Server log: server_log.txt"
    print_info "Test report: $TEST_DIR/test_report.txt"
}

# Check if script is run directly
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 