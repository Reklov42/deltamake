#!/bin/bash

# Colors
CReset="\033[0m"
CBlack="\033[0;30m"
CRed="\033[0;31m"
CGreen="\033[0;32m"
CYellow="\033[0;33m"
CBlue="\033[0;34m"
CPurple="\033[0;35m"
CCyan="\033[0;36m"

# Check jsoncpp
echo -n -e "Checking if "$CCyan"jsoncpp"$CReset" exists on this system... "
/sbin/ldconfig -p | grep jsoncpp > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo -e "["$CGreen"EXISTS"$CReset"] Yippee!"
else
    echo -e "["$CRed"DOES NOT EXIST"$CReset"] Scheisse!"
    echo -e "Install "$CPurple"jsoncpp"$CReset", dude..."
    exit 69
fi

# Automatically generated code
AutoGenPath=./source/AutoGen.h
touch $AutoGenPath
echo -e "// Automatically generated code\n" > $AutoGenPath

# Defines
echo -e "// Defines" >>  $AutoGenPath
# Commit
CommitDate=$(git --no-pager log -1 --pretty='format:%cd' --date='format:%d.%m.%Y')
echo -e "Commit date: "$CPurple$CommitDate$CReset
echo -e "#define DELTAMAKE_COMMIT_DATE \""$CommitDate"\"" >> $AutoGenPath
# Commit hash
CommitHash=$(git rev-parse --short HEAD)
echo -e "Commit hash: "$CBlue$CommitHash$CReset
echo -e "#define DELTAMAKE_COMMIT_HASH \""$CommitHash"\"\n" >> $AutoGenPath

# Building
echo -e "Building "$CGreen"deltamake"$CReset"..."
g++ --std=c++17 -g -ljsoncpp -I./source/ ./source/main.cpp -o deltamake
chmod +x ./deltamake
# Done
echo -e "AND NOW... "$CRed"BEWARE OF\n\n"$CReset
./deltamake -n -v
echo -e $CYellow"\n\nHave fun!"$CReset