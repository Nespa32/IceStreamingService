#!/bin/bash

set -e

pkill icebox || true
sleep 1

# wipe and recreate db
rm -rf db
mkdir -p db

icebox --Ice.Config=config.icebox
