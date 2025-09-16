#!/bin/bash

#
# Supermodel
# A Sega Model 3 Arcade Emulator.
# Copyright 2003-2025 The Supermodel Team
#
# This file is part of Supermodel.
#
# Supermodel is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Supermodel is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
#

#
# changelog.sh
#
# Produces a change log (i.e., CHANGES.txt attached with each release). Takes a
# single command line argument, which is the file path.
#

#!/bin/bash

# Check if file path argument is provided
if [ $# -ne 1 ]; then
    echo "Error: Please provide a file path as an argument" >&2
    echo "Usage: $0 <output_file_path>" >&2
    exit 1
fi

OUTPUT_FILE="$1"
FROM_REF=${FROM_REF:-06594a5}
REF=${REF:-$(git rev-parse --short HEAD)}

# Write header directly to output file
cat <<EOF > "${OUTPUT_FILE}"

  ####                                                      ###           ###
 ##  ##                                                      ##            ##
 ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##
  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##
    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##
 ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##
  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####
                 ####

                       A Sega Model 3 Arcade Emulator.

                   Copyright 2003-2025 The Supermodel Team

                                 CHANGE LOG

EOF

# Append git log directly to output file
if ! git -c color.ui=never log ${FROM_REF}...${REF} --no-decorate --format=format:"Commit %h on %ad by %an%n%n%w(80,4,4)%B" --date=short >> "${OUTPUT_FILE}" ; then
    echo "Unable to obtain Git log" >&2
    exit 1
fi