#!/bin/bash

set -e

pkill icebox || true
sleep 1
icebox --Ice.Config=config.icebox
