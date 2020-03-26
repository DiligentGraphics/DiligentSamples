#!/bin/bash
echo "Format validation is not yet enabled for DiligentSamples module on MacOS"
python ../../../DiligentCore/BuildTools/FormatValidation/clang-format-validate.py \
--clang-format-executable ../../../DiligentCore/BuildTools/FormatValidation/clang-format_10.0.0 \
-r ../../SampleBase ../../Tutorials ../../Samples \
--exclude ../../SampleBase/src/UWP \
--exclude ../../SampleBase/src/Win32/resources
