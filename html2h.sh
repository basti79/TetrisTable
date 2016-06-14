#!/bin/bash

echo -n 'const char index_html[]="' >index_html.h
perl -ne 's/"/\\"/g; chomp; print;' <index.html >>index_html.h
echo '";' >>index_html.h
