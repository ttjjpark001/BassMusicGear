#!/usr/bin/env bash
set -euo pipefail

# CleanBuild.sh — build/ 디렉터리를 완전 삭제한 뒤 CMake를 재구성한다.
#
# 사용법:
#   ./scripts/CleanBuild.sh           # Debug (기본값)
#   ./scripts/CleanBuild.sh Release   # Release
#   ./scripts/CleanBuild.sh Debug     # Debug 명시

CONFIG="${1:-Debug}"

# 허용된 설정값 검증
if [[ "${CONFIG}" != "Debug" && "${CONFIG}" != "Release" ]]; then
  echo "[ERROR] CONFIG는 Debug 또는 Release여야 합니다. 입력값: '${CONFIG}'"
  exit 1
fi

# 프로젝트 루트로 이동 (스크립트가 어디서 호출되어도 동작하도록)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

echo ">>> 프로젝트 루트: ${PROJECT_ROOT}"

# build/ 디렉터리 삭제
if [ -d "build" ]; then
  echo ">>> build/ 디렉터리를 삭제합니다..."
  rm -rf build/
  echo ">>> 삭제 완료."
else
  echo ">>> build/ 디렉터리가 없습니다. 삭제를 건너뜁니다."
fi

# CMake 재구성
echo ">>> CMake 구성 중 (${CONFIG})..."
cmake -B build -DCMAKE_BUILD_TYPE="${CONFIG}"

echo ""
echo ">>> 완료. 이제 다음 명령으로 빌드를 시작하세요:"
echo "    cmake --build build --config ${CONFIG}"
