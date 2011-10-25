#include <stdio.h>

// Note: kazlib has no advertisement clause so for now at least we
// do not mention it.

static char self_license[] = "\
Copyright (c) 2002-2011 David Boyce.  All rights reserved.\n\
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

static char curl_license[] = "\
COPYRIGHT AND PERMISSION NOTICE\n\
\n\
Copyright (c) 1996 - 2008, Daniel Stenberg, <daniel@haxx.se>.\n\
\n\
All rights reserved.\n\
\n\
Permission to use, copy, modify, and distribute this software for any purpose\n\
with or without fee is hereby granted, provided that the above copyright\n\
notice and this permission notice appear in all copies.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN\n\
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,\n\
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR\n\
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE\n\
OR OTHER DEALINGS IN THE SOFTWARE.\n\
\n\
Except as contained in this notice, the name of a copyright holder shall not\n\
be used in advertising or otherwise to promote the sale, use or other dealings\n\
in this Software without prior written authorization of the copyright holder.\n\
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

static char zlib_license[] = "\
Copyright (C) 1995-2003 Jean-loup Gailly and Mark Adler\n\
\n\
This software is provided 'as-is', without any express or implied\n\
warranty.  In no event will the authors be held liable for any damages\n\
arising from the use of this software.\n\
\n\
Permission is granted to anyone to use this software for any purpose,\n\
including commercial applications, and to alter it and redistribute it\n\
freely, subject to the following restrictions:\n\
\n\
1. The origin of this software must not be misrepresented; you must not\n\
   claim that you wrote the original software. If you use this software\n\
   in a product, an acknowledgment in the product documentation would be\n\
   appreciated but is not required.\n\
2. Altered source versions must be plainly marked as such, and must not be\n\
   misrepresented as being the original software.\n\
3. This notice may not be removed or altered from any source distribution.\n\
\n\
Jean-loup Gailly        Mark Adler\n\
jloup@gzip.org          madler@alumni.caltech.edu\n\
";

static char pcre_license[] = "\
PCRE LICENCE\n\
------------\n\
\n\
PCRE is a library of functions to support regular expressions whose syntax\n\
and semantics are as close as possible to those of the Perl 5 language.\n\
\n\
Release 6 of PCRE is distributed under the terms of the 'BSD' licence, as\n\
specified below. The documentation for PCRE, supplied in the 'doc'\n\
directory, is distributed under the same terms as the software itself.\n\
\n\
The basic library functions are written in C and are freestanding. Also\n\
included in the distribution is a set of C++ wrapper functions.\n\
\n\
\n\
THE BASIC LIBRARY FUNCTIONS\n\
---------------------------\n\
\n\
Written by:       Philip Hazel\n\
Email local part: ph10\n\
Email domain:     cam.ac.uk\n\
\n\
University of Cambridge Computing Service,\n\
Cambridge, England. Phone: +44 1223 334714.\n\
\n\
Copyright (c) 1997-2008 University of Cambridge\n\
All rights reserved.\n\
\n\
\n\
THE C++ WRAPPER FUNCTIONS\n\
-------------------------\n\
\n\
Contributed by:   Google Inc.\n\
\n\
Copyright (c) 2005, Google Inc.\n\
All rights reserved.\n\
\n\
\n\
THE 'BSD' LICENCE\n\
-----------------\n\
"

// Windows has a dumb restriction on the length of literal strings ...

"\n\
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
    * Neither the name of the University of Cambridge nor the name of Google\n\
      Inc. nor the names of their contributors may be used to endorse or\n\
      promote products derived from this software without specific prior\n\
      written permission.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS'\n\
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
\n\
End\n\
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

static char banner[] =
"@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@";

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

void
about_client(void)
{
    print_license("CURL", curl_license);

    print_license("TRIO", trio_license);

    print_license("ZLIB", zlib_license);

    print_license("PCRE", pcre_license);

    print_license("BSD", bsd_license);

#if defined(_WIN32)
#include "About/about_win.c"
#endif
}
