#!/usr/bin/env python
#     Copyright 2019, Kay Hayen, mailto:kay.hayen@gmail.com
#
#     Python test originally created or extracted from other peoples work. The
#     parts from me are licensed as below. It is at least Free Software where
#     it's copied from other people. In these cases, that will normally be
#     indicated.
#
#     Licensed under the Apache License, Version 2.0 (the "License");
#     you may not use this file except in compliance with the License.
#     You may obtain a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#     Unless required by applicable law or agreed to in writing, software
#     distributed under the License is distributed on an "AS IS" BASIS,
#     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#     See the License for the specific language governing permissions and
#     limitations under the License.
#

""" Runner for package tests of Nuitka.

Package tests are typically aiming at checking specific module constellations
in module mode and making sure the details are being right there. These are
synthetic small packages, each of which try to demonstrate one or more points
or special behaviour.

"""


import os
import sys

# Find nuitka package relative to us.
sys.path.insert(
    0,
    os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
    ),
)

# isort:start

from nuitka.tools.testing.Common import (
    compareWithCPython,
    createSearchMode,
    getTempDir,
    my_print,
    setup,
    withExtendedExtraOptions,
)


def main():
    _python_version = setup()

    search_mode = createSearchMode()

    for filename in sorted(os.listdir(".")):
        if not os.path.isdir(filename) or filename.endswith(".build"):
            continue

        extra_flags = [
            "expect_success",
            "remove_output",
            "module_mode",
            "two_step_execution",
        ]

        # The use of "__main__" in the test package gives a warning.
        if filename == "sub_package":
            extra_flags.append("ignore_warnings")

        active = search_mode.consider(dirname=None, filename=filename)

        if active:
            my_print("Consider output of recursively compiled program:", filename)

            filename_main = None
            for filename_main in os.listdir(filename):
                if not os.path.isdir(os.path.join(filename, filename_main)):
                    continue

                if filename_main not in ("..", "."):
                    break
            else:
                search_mode.onErrorDetected(
                    """\
Error, no package in dir '%s' found, incomplete test case."""
                    % filename
                )

            extensions = ["--include-package=%s" % os.path.basename(filename_main)]

            if "--output-dir" not in os.environ.get("NUITKA_EXTRA_OPTIONS", ""):
                extensions.append("--output-dir=%s" % getTempDir())

            with withExtendedExtraOptions(*extensions):
                compareWithCPython(
                    dirname=filename,
                    filename=filename_main,
                    extra_flags=extra_flags,
                    search_mode=search_mode,
                    needs_2to3=False,
                )

            if search_mode.abortIfExecuted():
                break
        else:
            my_print("Skipping", filename)

    search_mode.finish()


if __name__ == "__main__":
    main()
