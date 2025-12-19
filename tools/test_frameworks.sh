#!/bin/bash
# Test compilation of UDP server component with both Arduino and ESP-IDF frameworks

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "UDP Server Component - Framework Tests"
echo "========================================="
echo

# Activate venv if it exists
if [ -z "$VIRTUAL_ENV" ]; then
  if [ -f ".venv/bin/activate" ]; then
    echo "Activating virtual environment..."
    source .venv/bin/activate
  elif [ -f "venv/bin/activate" ]; then
    echo "Activating virtual environment..."
    source venv/bin/activate
  else
    echo -e "${YELLOW}Warning: venv not found, assuming esphome is in PATH${NC}"
  fi
fi

# Check ESPHome is available
if ! command -v esphome &> /dev/null; then
    echo -e "${RED}ERROR: esphome not found. Please install ESPHome.${NC}"
    exit 1
fi

echo "ESPHome version: $(esphome version)"
echo

# Test Arduino framework
echo "========================================="
echo "Test 1: Arduino Framework Build"
echo "========================================="
echo "Building examples/test_arduino.yaml..."
echo

if esphome compile examples/test_arduino.yaml; then
    echo
    echo -e "${GREEN}✓ Arduino framework build PASSED${NC}"
    echo
else
    echo
    echo -e "${RED}✗ Arduino framework build FAILED${NC}"
    echo
    exit 1
fi

# Test ESP-IDF framework
echo "========================================="
echo "Test 2: ESP-IDF Framework Build"
echo "========================================="
echo "Building examples/test_idf.yaml..."
echo

if esphome compile examples/test_idf.yaml; then
    echo
    echo -e "${GREEN}✓ ESP-IDF framework build PASSED${NC}"
    echo
else
    echo
    echo -e "${RED}✗ ESP-IDF framework build FAILED${NC}"
    echo
    exit 1
fi

# Summary
echo "========================================="
echo "Summary"
echo "========================================="
echo -e "${GREEN}✓ All tests PASSED${NC}"
echo
echo "Both Arduino and ESP-IDF frameworks compiled successfully!"
echo "The component is ready for dual-framework support."
echo

# Show build artifacts
echo "Build artifacts:"
echo "  Arduino: .esphome/build/udpserver-arduino-test/"
echo "  ESP-IDF: .esphome/build/udpserver-idf-test/"
echo
