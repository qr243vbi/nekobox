#!/bin/bash

#!/bin/bash 

### Builds NekoBox with your local changes via GitHub Actions (recommended for Windows).

### Usage (Linux/macOS/Git Bash):

### chmod +x ./script/publish_and_build.sh

### ./script/publish_and_build.sh

set -e # Stop execution on any error 

### Get repo root directory relative to this script

SCRIPT_DIR="$(cd "(dirname "${BASH_SOURCE}")" && pwd)"
REPO_ROOT="(dirname "SCRIPT_DIR")"
cd "$REPO_ROOT" 

### ANSI color codes

CYAN='\033[0;36m'
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color 

echo -e ""
echo -e "${CYAN}=== NekoBox: build via GitHub Actions ===${NC}"
echo -e ""
echo -e "Local build on Windows requires Visual Studio, Qt 6, vcpkg, and ~10+ GB."
echo -e "Easier way: push your changes to fork and build in the GitHub cloud"
echo -e "" 

status=$(git status --porcelain)
if [ -n "$status" ]; then
echo -e "${YELLOW}Uncommitted changes:${NC}"
git status -sb
echo -e ""
fi 

read -p "Your GitHub username (login, not email): " username
if [ -z "$username" ]; then
echo -e "${RED}Error: Username is required.${NC}"
exit 1
fi 

forkUrl="https://github.com/$username/nekobox.git"
echo -e ""
echo -e "${GREEN}1) If the fork does not exist yet, open it in your browser:${NC}"
echo -e "   https://github.com/qr243vbi/nekobox/fork"
echo -e "   Click 'Create fork' and wait for it to be created."
echo -e ""
read -p "Press Enter when the fork is ready" 

if [ -n "$status" ]; then
read -p "Commit current changes before pushing? (y/n): " doCommit
if [[ "$doCommit" =~ ^[yY] ]]; then
git add -A
read -p "Commit message (Enter = default): " msg
if [ -z "$msg" ]; then
msg="Add log error filter and quick route add from log"
fi
git commit -m "$msg"
echo -e "${GREEN}Commit created.${NC}"
else
echo -e "${YELLOW}Pushing without a new commit — old files will remain on GitHub${NC}"
fi
fi 

if git remote | grep -q "^fork$"; then
git remote set-url fork "$forkUrl"
echo -e "Updated remote 'fork' -> $forkUrl"
else
git remote add fork "$forkUrl"
echo -e "Added remote 'fork' -> $forkUrl"
fi 

echo -e ""
echo -e "${GREEN}2) Pushing to your fork (may ask for GitHub login)...${NC}" 

if ! git push -u fork HEAD:main; then
echo -e ""
echo -e "${RED}Push failed. Common reasons:${NC}"
echo -e "  - fork was not created"
echo -e "  - no access (Personal Access Token is needed instead of a password)"
echo -e "  - the fork already has a different history (try: git pull fork main --rebase)"
exit 1
fi 

actionsUrl="https://github.com/$username/nekobox/actions/workflows/build.yml"
echo -e ""
echo -e "${GREEN}3) Starting the build:${NC}"
echo -e "   $actionsUrl"
echo -e ""
echo -e "   - Run workflow -> Run workflow"
echo -e "   - publish: false (so it does not publish a release)"
echo -e "   - build_windows_x64: true, other Windows variants optional"
echo -e "   - Wait for the green checkmark (~30-60 mins)"
echo -e ""
echo -e "4) Download the artifact 'nekobox-...-windows-2022-x64...'"
echo -e "   Inside the ZIP/portable archive is nekobox.exe with your changes."
echo -e "" 

### Cross-platform URL opener (works on macOS, Linux, and WSL/Git Bash)

if command -v xdg-open &> /dev/null; then
xdg-open "$actionsUrl" &> /dev/null &
elif command -v open &> /dev/null; then
open "$actionsUrl"
elif command -v explorer.exe &> /dev/null; then
explorer.exe "$actionsUrl"
fi 

echo -e "${CYAN}Actions page opened in the browser.${NC}"
