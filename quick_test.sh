#!/bin/bash

# Quick Test Script for Multi-threaded Chat Server
# This script performs basic functionality tests quickly

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SERVER_PORT=8080
SERVER_PID=""

print_status() {
    echo -e "${1}[$(date '+%H:%M:%S')] ${2}${NC}"
}

print_success() { print_status "$GREEN" "✅ $1"; }
print_error() { print_status "$RED" "❌ $1"; }
print_info() { print_status "$BLUE" "ℹ️  $1"; }

cleanup() {
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill -TERM "$SERVER_PID" 2>/dev/null || true
        sleep 1
        if kill -0 "$SERVER_PID" 2>/dev/null; then
            kill -KILL "$SERVER_PID" 2>/dev/null || true
        fi
    fi
    pkill -f "chatserver" 2>/dev/null || true
    pkill -f "chatclient" 2>/dev/null || true
}

trap cleanup EXIT

echo -e "\n${BLUE}=== QUICK TEST SUITE ===${NC}"

# Test 1: Compilation
print_info "Testing compilation..."
if make clean > /dev/null 2>&1 && make all > /dev/null 2>&1; then
    print_success "Compilation test passed"
else
    print_error "Compilation test failed"
    exit 1
fi

# Test 2: Server startup
print_info "Testing server startup..."
./chatserver $SERVER_PORT > /dev/null 2>&1 &
SERVER_PID=$!
sleep 2

if kill -0 "$SERVER_PID" 2>/dev/null; then
    print_success "Server startup test passed"
else
    print_error "Server startup test failed"
    exit 1
fi

# Test 3: Client connection
print_info "Testing client connection..."
timeout 5 bash -c "echo 'testuser' | ./chatclient 127.0.0.1 $SERVER_PORT" > /dev/null 2>&1 &
sleep 3

if grep -q "LOGIN.*testuser.*connected" server_log.txt 2>/dev/null; then
    print_success "Client connection test passed"
else
    print_error "Client connection test failed"
fi

# Test 4: Logging functionality
print_info "Testing logging functionality..."
if [[ -f "server_log.txt" ]] && [[ -s "server_log.txt" ]]; then
    print_success "Logging test passed"
    print_info "Log entries: $(wc -l < server_log.txt)"
else
    print_error "Logging test failed"
fi

# Test 5: File transfer system initialization
print_info "Testing file transfer system..."
if grep -q "FILE-INIT.*File transfer system initialized" server_log.txt 2>/dev/null; then
    print_success "File transfer system test passed"
else
    print_error "File transfer system test failed"
fi

# Test 6: Graceful shutdown
print_info "Testing graceful shutdown..."
kill -INT "$SERVER_PID"
sleep 2

if grep -q "SHUTDOWN.*SIGINT received" server_log.txt 2>/dev/null; then
    print_success "Graceful shutdown test passed"
else
    print_error "Graceful shutdown test failed"
fi

echo -e "\n${GREEN}=== QUICK TEST COMPLETED ===${NC}"
print_info "For comprehensive testing, run: ./test_suite.sh"
print_info "Server log: server_log.txt" 