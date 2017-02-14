#!/bin/sh

( cd src; sh MMM.sh )
rm -f latest.zip
zip -r latest.zip released


git add . -A
git commit
git push
