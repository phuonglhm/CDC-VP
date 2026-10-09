#!/bin/bash

# Thiết lập màu sắc
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}   H.264 VIDEO PROCESSING - BUILD & RUN ALL       ${NC}"
echo -e "${BLUE}==================================================${NC}\n"

BLOCKS=("tq" "ec" "df" "mem")
BASE_DIR=$(pwd)

for block in "${BLOCKS[@]}"; do
    block_upper=$(echo "$block" | tr '[:lower:]' '[:upper:]')
    
    echo -e "${YELLOW}[BUILDING] Đang cập nhật và biên dịch khối ${block_upper}...${NC}"
    
    # Nhảy vào thư mục test của từng block
    cd "${BASE_DIR}/${block}/test" || exit
    
    # Chạy lệnh build (ẩn bớt log cấu hình CMake cho đỡ rối mắt)
    cmake -B build > /dev/null 2>&1 
    cmake --build build
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}[FAILED] Lỗi biên dịch tại khối ${block_upper}! Hãy check lại code.${NC}\n"
        exit 1
    fi
    
    echo -e "${BLUE}[TESTING] Đang chạy Unit Test cho khối ${block_upper}...${NC}"
    ./build/test_h264_vp_${block}
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}[SUCCESS] Khối ${block_upper} PASSED toàn bộ kịch bản!${NC}\n"
    else
        echo -e "${RED}[FAILED] Khối ${block_upper} gặp lỗi trong quá trình test!${NC}\n"
        exit 1
    fi
done

echo -e "${GREEN}==================================================${NC}"
echo -e "${GREEN} ĐÃ BUILD VÀ TEST THÀNH CÔNG 4 KHỐI (TQ, EC, DF, MEM) ${NC}"
echo -e "${GREEN}==================================================${NC}"