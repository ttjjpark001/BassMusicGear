#!/usr/bin/env bash
set -euo pipefail

# GenBinaryData.sh
# Resources/IR/와 Resources/Presets/ 디렉터리를 스캔하여
# CMakeLists.txt의 juce_add_binary_data 블록 내 SOURCES 목록을 자동 갱신한다.
#
# 사용: ./scripts/GenBinaryData.sh
# 갱신 전후 diff를 출력하여 변경 내역 확인 가능

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CMAKE_FILE="${PROJECT_ROOT}/CMakeLists.txt"
RESOURCES_DIR="${PROJECT_ROOT}/Resources"

# 스캔할 파일 경로들을 수집
declare -a FILES_TO_ADD

echo ">>> Scanning Resources/IR/ and Resources/Presets/..."

# Resources/IR/*.wav 수집
if [ -d "$RESOURCES_DIR/IR" ]; then
    while IFS= read -r -d '' file; do
        # 상대 경로로 변환 (Resources/... 형태)
        rel_path="${file#$PROJECT_ROOT/}"
        FILES_TO_ADD+=("$rel_path")
        echo "  [IR] $rel_path"
    done < <(find "$RESOURCES_DIR/IR" -maxdepth 1 -name "*.wav" -print0 2>/dev/null | sort -z)
fi

# Resources/Presets/*.xml 수집
if [ -d "$RESOURCES_DIR/Presets" ]; then
    while IFS= read -r -d '' file; do
        rel_path="${file#$PROJECT_ROOT/}"
        FILES_TO_ADD+=("$rel_path")
        echo "  [Presets] $rel_path"
    done < <(find "$RESOURCES_DIR/Presets" -maxdepth 1 -name "*.xml" -print0 2>/dev/null | sort -z)
fi

# 플레이스홀더 파일들 추가
if [ -f "$RESOURCES_DIR/placeholder.txt" ]; then
    FILES_TO_ADD+=("Resources/placeholder.txt")
    echo "  [Placeholder] Resources/placeholder.txt"
fi

# 파일이 없으면 placeholder.txt 추가 (BinaryData SOURCES가 비어 있으면 빌드 오류 발생)
if [ ${#FILES_TO_ADD[@]} -eq 0 ]; then
    FILES_TO_ADD+=("Resources/placeholder.txt")
    echo "  [Placeholder] Resources/placeholder.txt (fallback)"
fi

# 백업 생성
BACKUP_FILE="${CMAKE_FILE}.backup"
cp "$CMAKE_FILE" "$BACKUP_FILE"
echo ""
echo ">>> Backup created: $BACKUP_FILE"

# CMakeLists.txt에서 juce_add_binary_data 블록 찾고 SOURCES 라인들을 교체
# AWK를 사용하여 블록 내의 SOURCES부터 다음 닫는 괄호까지 교체

TMP_FILE="${CMAKE_FILE}.tmp"

# Python을 사용하여 더 안정적으로 블록 교체
python3 - "$CMAKE_FILE" "${FILES_TO_ADD[@]}" <<'PYTHON_EOF'
import re
import sys

cmake_file = sys.argv[1]
files_list = sys.argv[2:]

# CMakeLists.txt 읽기
with open(cmake_file, 'r') as f:
    content = f.read()

# juce_add_binary_data 블록 찾기
# juce_add_binary_data(BassMusicGear_BinaryData
#     SOURCES
#         Resources/...
# )
pattern = r'(juce_add_binary_data\(BassMusicGear_BinaryData\s*SOURCES\s*)(.*?)(\n\))'

def replace_sources(match):
    prefix = match.group(1)
    closing = match.group(3)

    # 새 SOURCES 항목 생성
    if files_list:
        sources_lines = "\n        ".join(files_list)
        return f"{prefix}\n        {sources_lines}{closing}"
    else:
        return f"{prefix}\n        Resources/placeholder.txt{closing}"

new_content = re.sub(pattern, replace_sources, content, flags=re.DOTALL)

# 결과 저장
with open(cmake_file, 'w') as f:
    f.write(new_content)

print(">>> Updated CMakeLists.txt SOURCES block")

PYTHON_EOF

# 오류 체크
if [ $? -ne 0 ]; then
    echo ">>> ERROR: Failed to update CMakeLists.txt"
    cp "$BACKUP_FILE" "$CMAKE_FILE"
    echo ">>> Restored from backup"
    exit 1
fi

# Diff 출력
echo ""
echo ">>> Changes:"
diff -u "$BACKUP_FILE" "$CMAKE_FILE" || true

echo ""
echo ">>> Done. CMakeLists.txt SOURCES updated with ${#FILES_TO_ADD[@]} file(s)"
