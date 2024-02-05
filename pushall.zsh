#!/bin/zsh
for i in $(seq 20000 -900 0)
do
   git push origin main~$i:main
done
git push origin main
