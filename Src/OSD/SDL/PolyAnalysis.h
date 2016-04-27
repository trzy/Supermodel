/**
 ** Supermodel
 ** A Sega Model 3 Arcade Emulator.
 ** Copyright 2011-2016 Bart Trzynadlowski, Nik Henson 
 **
 ** This file is part of Supermodel.
 **
 ** Supermodel is free software: you can redistribute it and/or modify it under
 ** the terms of the GNU General Public License as published by the Free 
 ** Software Foundation, either version 3 of the License, or (at your option)
 ** any later version.
 **
 ** Supermodel is distributed in the hope that it will be useful, but WITHOUT
 ** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 ** FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 ** more details.
 **
 ** You should have received a copy of the GNU General Public License along
 ** with Supermodel.  If not, see <http://www.gnu.org/licenses/>.
 **/

/*
 * PolyAnalysis.h
 *
 * HTML GUI used to analyze polygon header bits.
 */

#ifndef INCLUDED_POLYANALYSIS_H
#define INCLUDED_POLYANALYSIS_H

static const char s_polyAnalysisHTMLPrologue[] =
{
  "<html>\n"
  "<head>\n"
  "  <style>\n"
  "    table { table-layout: fixed; }\n"
  "    td { border: 1px solid black; width: 1em; }\n"
  "    td.nonselectable { background-color: gray; }\n"
  "    td.selectable { background-color: #5555ee; }\n"
  "    td.selected { background-color: #22ff22; }\n"
  "  </style>\n"
  "  <script src='https://ajax.googleapis.com/ajax/libs/jquery/2.2.2/jquery.min.js'></script>\n"
  "</head>\n"
  "<body>\n"
  "  <div id='Header'></div>\n"
  "  <div><img id='Image' /></div>\n"
  "  <script>\n"
  // Insert g_file_base_name and g_unknown bits here, then append epilogue
};

static const char s_polyAnalysisHTMLEpilogue[] =
{
  "    function hex32(val)\n"
  "    {\n"
  "      return '0x' + ('0000000'+Number(val).toString(16)).slice(-8);\n"
  "    }\n"
  "    function getOnClickHandler(table, td, word, mask)\n"
  "    {\n"
  "      return function()\n"
  "      {\n"
  "        var file = g_file_base_name + '.' + word + '_' + hex32(mask) + '.bmp';\n"
  "        table.find('td').removeClass('selected'); // remove from all cells\n"
  "        td.addClass('selected');\n"
  "        $('#Image').empty();\n"
  "        $('#Image').attr('src', file);\n"
  "      };\n"
  "    }\n"
  "    $(document).ready(function()\n"
  "    {\n"
  "      // Construct table\n"
  "      var table = $('<table></table>');\n"
  "      var bits_tr = $('<tr></tr>');   // first row will be bit legend\n"
  "      bits_tr.append($('<td></td>')); // column for word numbers\n"
  "      for (var bit = 31; bit >= 0; bit--)\n"
  "        bits_tr.append($('<td>'+bit+'</td>'));\n"
  "      table.append(bits_tr);\n"
  "      for (var word = 0; word < 7; word++)\n"
  "      {\n"
  "        var tr = $('<tr></tr>');\n"
  "        tr.append($('<td>'+word+'</td>'));\n"
  "        for (var bit = 31; bit >= 0; bit--)\n"
  "        {\n"
  "          var mask = 1 << bit;\n"
  "          var td = $('<td></td>');\n"
  "          if (g_unknown_bits[word] & mask)\n"
  "            td.addClass('selectable').click(getOnClickHandler(table, td, word, mask));\n"
  "          else\n"
  "            td.addClass('nonselectable');\n"
  "          tr.append(td);\n"
  "        }\n"
  "        table.append(tr);\n"
  "      }\n"
  "      $('#Header').append(table);\n"
  "    });\n"
  "  </script>\n"
  "</body>\n"
};

#endif  // INCLUDED_POLYANALYSIS_H
