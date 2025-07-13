#include <stdio.h>

// Note: kazlib has no advertisement clause so for now at least we
// do not mention it.

static char self_license[] = "\
Copyright (c) 2002-2025 David Boyce.  All rights reserved.\n\
Warning: this computer software is protected by copyright law and\n\
international treaties. Unauthorized reproduction or distribution of\n\
this software, or any portion of it, may result in severe civil and\n\
criminal penalties, and will be prosecuted to the maximum extent\n\
possible under the law.\n\
\n\
This program is free software: you can redistribute it and/or modify\n\
it under the terms of the GNU Affero General Public License as\n\
published by the Free Software Foundation, either version 3 of the\n\
License, or (at your option) any later version.\n\
\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU Affero General Public License for more details.\n\
\n\
You should have received a copy of the GNU Affero General Public License\n\
along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\
";

static char bsd_license[] = "\
Copyright (c) 1990, 2008\n\
    The Regents of the University of California.  All rights reserved.\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions\n\
are met:\n\
1. Redistributions of source code must retain the above copyright\n\
notice, this list of conditions and the following disclaimer.\n\
2. Redistributions in binary form must reproduce the above copyright\n\
notice, this list of conditions and the following disclaimer in the\n\
documentation and/or other materials provided with the distribution.\n\
3. All advertising materials mentioning features or use of this software\n\
must display the following acknowledgement:\n\
    This product includes software developed by the University of\n\
    California, Berkeley and its contributors.\n\
4. Neither the name of the University nor the names of its contributors\n\
may be used to endorse or promote products derived from this software\n\
without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND\n\
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE\n\
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n\
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n\
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n\
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n\
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n\
SUCH DAMAGE.\n\
";

static char cjson_license[] = "\
Copyright (c) 2009-2017 Dave Gamble and cJSON contributors\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in\n\
all copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n\
THE SOFTWARE.\n\
";

static char pcre2_license[] = "\
           Copyright (c) 2016-2024 University of Cambridge\n\
\n\
-----------------------------------------------------------------------------\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
\n\
    * Redistributions of source code must retain the above copyright notice,\n\
      this list of conditions and the following disclaimer.\n\
\n\
    * Redistributions in binary form must reproduce the above copyright\n\
      notice, this list of conditions and the following disclaimer in the\n\
      documentation and/or other materials provided with the distribution.\n\
\n\
    * Neither the name of the University of Cambridge nor the names of its\n\
      contributors may be used to endorse or promote products derived from\n\
      this software without specific prior written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n\
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n\
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n\
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n\
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n\
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n\
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n\
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n\
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n\
POSSIBILITY OF SUCH DAMAGE.\n\
";

static char trio_license[] = "\
Trio is a package with portable string functions. Including printf() clones\n\
and others.\n\
\n\
 Copyright (C) 1998-2006 by Bjorn Reese and Daniel Stenberg.\n\
\n\
 Permission to use, copy, modify, and distribute this software for any\n\
 purpose with or without fee is hereby granted, provided that the above\n\
 copyright notice and this permission notice appear in all copies.\n\
\n\
 THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED\n\
 WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF\n\
 MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS AND\n\
 CONTRIBUTORS ACCEPT NO RESPONSIBILITY IN ANY CONCEIVABLE MANNER.\n\
";

static char xxhash_license[] = "\
Copyright (C) 2012-2023 Yann Collet\n\
\n\
BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are\n\
met:\n\
\n\
   * Redistributions of source code must retain the above copyright\n\
     notice, this list of conditions and the following disclaimer.\n\
   * Redistributions in binary form must reproduce the above\n\
     copyright notice, this list of conditions and the following disclaimer\n\
     in the documentation and/or other materials provided with the\n\
     distribution.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n\
\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n\
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n\
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n\
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n\
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n\
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n\
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n\
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n\
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n\
\n\
You can contact the author at:\n\
  - xxHash homepage: https://www.xxhash.com\n\
  - xxHash source repository: https://github.com/Cyan4973/xxHash\n\
";

static char banner[] =
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";

void print_license(const char *, const char *);

void
print_license(const char *title, const char *text)
{
    fputs(banner, stdout);
    fputs("\n\t\t\t\t", stdout);
    fputs(title, stdout);
    fputs("\n", stdout);
    fputs(banner, stdout);
    fputs("\n\n", stdout);
    fputs(text, stdout);
    fputs("\n\n", stdout);
}

void about_client(void);

void
about_client(void)
{
    print_license("BSD", bsd_license);
    print_license("cJSON", cjson_license);
    print_license("PCRE", pcre2_license);
    print_license("TRIO", trio_license);
    print_license("XXHASH", xxhash_license);
}

// vim: sw=4 ts=8 tw=100 cc=+1
