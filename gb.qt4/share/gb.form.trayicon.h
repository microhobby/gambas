/***************************************************************************

  gb.form.trayicon.h

  (c) 2000-2013 Benoît Minisini <gambas@users.sourceforge.net>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
  MA 02110-1301, USA.

***************************************************************************/

#define DEFAULT_TRAYICON_WIDTH 24
#define DEFAULT_TRAYICON_HEIGHT 24

#ifdef QT_INTERFACE_VERSION

static const unsigned char _default_trayicon_data[] =
{
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 19, 12, 3, 80, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 6, 
33, 23, 9, 54, 35, 28, 9, 180, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 25, 0, 0, 0, 69, 
0, 0, 0, 78, 0, 0, 0, 37, 
0, 0, 0, 2, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 3, 0, 0, 0, 11, 
0, 0, 0, 10, 33, 25, 7, 108, 
63, 48, 16, 172, 70, 55, 18, 198, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 4, 0, 0, 0, 61, 
3, 3, 3, 196, 35, 35, 35, 225, 
49, 49, 49, 230, 7, 7, 7, 202, 
0, 0, 0, 102, 0, 0, 0, 9, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 20, 10, 0, 25, 
18, 14, 3, 138, 16, 12, 4, 123, 
30, 23, 6, 150, 125, 98, 32, 235, 
121, 94, 31, 242, 95, 74, 24, 211, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 6, 
4, 4, 4, 103, 59, 59, 59, 236, 
210, 210, 188, 254, 254, 254, 212, 255, 
254, 254, 224, 255, 234, 234, 229, 254, 
124, 124, 124, 250, 41, 41, 41, 111, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 1, 45, 30, 7, 34, 
78, 61, 19, 104, 114, 89, 29, 216, 
84, 65, 21, 232, 123, 95, 31, 238, 
159, 123, 41, 248, 193, 151, 50, 254, 
179, 139, 46, 254, 91, 71, 22, 179, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 23, 
75, 75, 75, 170, 209, 209, 189, 252, 
237, 237, 110, 255, 167, 167, 71, 255, 
187, 187, 124, 255, 254, 254, 245, 255, 
248, 248, 248, 255, 132, 132, 132, 225, 
58, 47, 15, 96, 62, 48, 16, 94, 
80, 61, 18, 108, 130, 102, 34, 195, 
157, 123, 40, 239, 188, 147, 49, 254, 
173, 135, 45, 254, 193, 150, 50, 255, 
194, 151, 50, 255, 193, 150, 50, 255, 
147, 115, 38, 245, 74, 58, 16, 92, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 32, 29, 21, 70, 
152, 151, 148, 222, 250, 250, 139, 255, 
141, 141, 31, 255, 20, 20, 7, 255, 
147, 147, 141, 255, 254, 254, 254, 255, 
255, 255, 255, 255, 226, 226, 226, 255, 
138, 108, 37, 239, 168, 130, 43, 238, 
171, 133, 44, 243, 189, 148, 49, 254, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 188, 146, 49, 255, 
120, 95, 31, 220, 54, 43, 10, 47, 
0, 0, 0, 0, 36, 24, 0, 21, 
82, 63, 20, 136, 130, 105, 49, 239, 
193, 187, 172, 254, 255, 255, 85, 255, 
48, 48, 10, 255, 1, 1, 1, 255, 
144, 144, 144, 255, 224, 224, 221, 255, 
253, 253, 247, 255, 254, 254, 254, 255, 
165, 136, 67, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
193, 151, 50, 255, 159, 123, 40, 250, 
92, 69, 22, 146, 18, 0, 0, 14, 
30, 30, 0, 17, 94, 72, 24, 138, 
166, 129, 43, 241, 184, 147, 58, 254, 
204, 193, 167, 255, 255, 255, 97, 255, 
72, 72, 15, 255, 0, 0, 0, 255, 
22, 22, 22, 255, 126, 126, 111, 255, 
251, 251, 238, 255, 251, 251, 251, 255, 
186, 149, 62, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 194, 151, 51, 255, 
180, 140, 46, 255, 113, 87, 28, 212, 
46, 34, 8, 60, 0, 0, 0, 0, 
66, 50, 16, 61, 139, 107, 35, 227, 
192, 149, 50, 254, 192, 150, 51, 255, 
199, 179, 130, 255, 244, 244, 164, 255, 
189, 189, 68, 255, 70, 70, 54, 255, 
41, 41, 27, 255, 194, 194, 157, 255, 
250, 248, 232, 255, 217, 202, 165, 255, 
194, 152, 52, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 185, 144, 48, 255, 
151, 117, 39, 253, 55, 41, 12, 124, 
0, 0, 0, 5, 0, 0, 0, 0, 
92, 72, 24, 176, 187, 146, 49, 254, 
195, 152, 51, 255, 195, 152, 51, 255, 
187, 148, 57, 255, 213, 198, 162, 255, 
244, 244, 222, 255, 252, 252, 229, 255, 
247, 242, 166, 255, 231, 213, 154, 255, 
212, 181, 109, 255, 201, 164, 74, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
188, 147, 49, 255, 157, 122, 40, 251, 
80, 63, 20, 186, 0, 0, 0, 3, 
0, 0, 0, 0, 0, 0, 0, 0, 
104, 81, 26, 200, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
182, 142, 47, 255, 164, 130, 50, 255, 
203, 181, 129, 255, 221, 203, 148, 255, 
214, 187, 115, 255, 203, 166, 81, 255, 
199, 159, 65, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 193, 150, 50, 255, 
163, 127, 42, 252, 105, 82, 28, 200, 
51, 38, 12, 59, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
107, 82, 26, 209, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
188, 146, 49, 255, 152, 118, 39, 255, 
127, 99, 34, 255, 181, 143, 52, 255, 
196, 154, 56, 255, 195, 152, 52, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
192, 149, 50, 255, 176, 137, 45, 255, 
192, 149, 50, 255, 174, 136, 45, 255, 
112, 87, 28, 213, 56, 43, 13, 77, 
0, 0, 0, 5, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
107, 85, 27, 216, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 194, 151, 50, 255, 
83, 69, 35, 255, 52, 49, 44, 255, 
57, 52, 46, 255, 76, 64, 47, 255, 
111, 90, 44, 255, 131, 105, 50, 255, 
107, 87, 44, 255, 72, 61, 34, 255, 
110, 87, 32, 255, 188, 146, 49, 255, 
165, 129, 43, 252, 120, 92, 31, 221, 
35, 27, 7, 64, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
99, 77, 25, 190, 191, 149, 50, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
144, 115, 50, 255, 50, 51, 114, 255, 
42, 48, 181, 255, 43, 49, 185, 255, 
45, 49, 135, 255, 48, 52, 142, 255, 
54, 58, 159, 255, 64, 61, 91, 255, 
158, 124, 47, 255, 160, 124, 41, 251, 
110, 86, 29, 201, 63, 49, 14, 88, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
84, 66, 20, 100, 162, 126, 42, 245, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
182, 142, 50, 255, 81, 67, 90, 255, 
6, 15, 221, 255, 3, 15, 254, 255, 
6, 17, 239, 255, 7, 18, 236, 255, 
14, 24, 221, 255, 88, 74, 84, 255, 
153, 120, 41, 255, 81, 63, 20, 185, 
52, 38, 14, 53, 0, 0, 0, 5, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
49, 35, 7, 36, 118, 92, 30, 194, 
185, 143, 48, 252, 194, 151, 50, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
194, 151, 50, 255, 180, 141, 48, 255, 
90, 74, 77, 255, 15, 21, 194, 255, 
6, 16, 221, 255, 12, 19, 212, 255, 
59, 51, 96, 251, 116, 90, 31, 226, 
68, 52, 17, 150, 0, 0, 0, 5, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 2, 47, 35, 7, 64, 
121, 94, 31, 205, 192, 149, 50, 254, 
195, 152, 51, 255, 195, 152, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
192, 149, 50, 255, 125, 100, 57, 255, 
103, 84, 83, 255, 73, 59, 58, 240, 
98, 75, 25, 168, 52, 39, 13, 78, 
21, 21, 0, 12, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 3, 
49, 35, 8, 57, 108, 84, 27, 203, 
166, 130, 43, 246, 194, 151, 51, 255, 
195, 152, 51, 255, 195, 152, 51, 255, 
188, 146, 49, 255, 134, 105, 35, 237, 
98, 78, 26, 196, 76, 59, 19, 90, 
31, 21, 0, 24, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 9, 
45, 36, 12, 85, 64, 50, 16, 142, 
75, 59, 18, 151, 75, 58, 18, 149, 
51, 39, 11, 115, 0, 0, 0, 22, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
};

#else

static const unsigned char _default_trayicon_data[] =
{
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 3, 12, 19, 80, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 6, 
9, 23, 33, 54, 9, 28, 35, 180, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 25, 0, 0, 0, 69, 
0, 0, 0, 78, 0, 0, 0, 37, 
0, 0, 0, 2, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 3, 0, 0, 0, 11, 
0, 0, 0, 10, 7, 25, 33, 108, 
16, 48, 63, 172, 18, 55, 70, 198, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 4, 0, 0, 0, 61, 
3, 3, 3, 196, 35, 35, 35, 225, 
49, 49, 49, 230, 7, 7, 7, 202, 
0, 0, 0, 102, 0, 0, 0, 9, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 10, 20, 25, 
3, 14, 18, 138, 4, 12, 16, 123, 
6, 23, 30, 150, 32, 98, 125, 235, 
31, 94, 121, 242, 24, 74, 95, 211, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 6, 
4, 4, 4, 103, 59, 59, 59, 236, 
188, 210, 210, 254, 212, 254, 254, 255, 
224, 254, 254, 255, 229, 234, 234, 254, 
124, 124, 124, 250, 41, 41, 41, 111, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 1, 7, 30, 45, 34, 
19, 61, 78, 104, 29, 89, 114, 216, 
21, 65, 84, 232, 31, 95, 123, 238, 
41, 123, 159, 248, 50, 151, 193, 254, 
46, 139, 179, 254, 22, 71, 91, 179, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 23, 
75, 75, 75, 170, 189, 209, 209, 252, 
110, 237, 237, 255, 71, 167, 167, 255, 
124, 187, 187, 255, 245, 254, 254, 255, 
248, 248, 248, 255, 132, 132, 132, 225, 
15, 47, 58, 96, 16, 48, 62, 94, 
18, 61, 80, 108, 34, 102, 130, 195, 
40, 123, 157, 239, 49, 147, 188, 254, 
45, 135, 173, 254, 50, 150, 193, 255, 
50, 151, 194, 255, 50, 150, 193, 255, 
38, 115, 147, 245, 16, 58, 74, 92, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 21, 29, 32, 70, 
148, 151, 152, 222, 139, 250, 250, 255, 
31, 141, 141, 255, 7, 20, 20, 255, 
141, 147, 147, 255, 254, 254, 254, 255, 
255, 255, 255, 255, 226, 226, 226, 255, 
37, 108, 138, 239, 43, 130, 168, 238, 
44, 133, 171, 243, 49, 148, 189, 254, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 49, 146, 188, 255, 
31, 95, 120, 220, 10, 43, 54, 47, 
0, 0, 0, 0, 0, 24, 36, 21, 
20, 63, 82, 136, 49, 105, 130, 239, 
172, 187, 193, 254, 85, 255, 255, 255, 
10, 48, 48, 255, 1, 1, 1, 255, 
144, 144, 144, 255, 221, 224, 224, 255, 
247, 253, 253, 255, 254, 254, 254, 255, 
67, 136, 165, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 151, 193, 255, 40, 123, 159, 250, 
22, 69, 92, 146, 0, 0, 18, 14, 
0, 30, 30, 17, 24, 72, 94, 138, 
43, 129, 166, 241, 58, 147, 184, 254, 
167, 193, 204, 255, 97, 255, 255, 255, 
15, 72, 72, 255, 0, 0, 0, 255, 
22, 22, 22, 255, 111, 126, 126, 255, 
238, 251, 251, 255, 251, 251, 251, 255, 
62, 149, 186, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 151, 194, 255, 
46, 140, 180, 255, 28, 87, 113, 212, 
8, 34, 46, 60, 0, 0, 0, 0, 
16, 50, 66, 61, 35, 107, 139, 227, 
50, 149, 192, 254, 51, 150, 192, 255, 
130, 179, 199, 255, 164, 244, 244, 255, 
68, 189, 189, 255, 54, 70, 70, 255, 
27, 41, 41, 255, 157, 194, 194, 255, 
232, 248, 250, 255, 165, 202, 217, 255, 
52, 152, 194, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 48, 144, 185, 255, 
39, 117, 151, 253, 12, 41, 55, 124, 
0, 0, 0, 5, 0, 0, 0, 0, 
24, 72, 92, 176, 49, 146, 187, 254, 
51, 152, 195, 255, 51, 152, 195, 255, 
57, 148, 187, 255, 162, 198, 213, 255, 
222, 244, 244, 255, 229, 252, 252, 255, 
166, 242, 247, 255, 154, 213, 231, 255, 
109, 181, 212, 255, 74, 164, 201, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
49, 147, 188, 255, 40, 122, 157, 251, 
20, 63, 80, 186, 0, 0, 0, 3, 
0, 0, 0, 0, 0, 0, 0, 0, 
26, 81, 104, 200, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
47, 142, 182, 255, 50, 130, 164, 255, 
129, 181, 203, 255, 148, 203, 221, 255, 
115, 187, 214, 255, 81, 166, 203, 255, 
65, 159, 199, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 50, 150, 193, 255, 
42, 127, 163, 252, 28, 82, 105, 200, 
12, 38, 51, 59, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
26, 82, 107, 209, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
49, 146, 188, 255, 39, 118, 152, 255, 
34, 99, 127, 255, 52, 143, 181, 255, 
56, 154, 196, 255, 52, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 149, 192, 255, 45, 137, 176, 255, 
50, 149, 192, 255, 45, 136, 174, 255, 
28, 87, 112, 213, 13, 43, 56, 77, 
0, 0, 0, 5, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
27, 85, 107, 216, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 50, 151, 194, 255, 
35, 69, 83, 255, 44, 49, 52, 255, 
46, 52, 57, 255, 47, 64, 76, 255, 
44, 90, 111, 255, 50, 105, 131, 255, 
44, 87, 107, 255, 34, 61, 72, 255, 
32, 87, 110, 255, 49, 146, 188, 255, 
43, 129, 165, 252, 31, 92, 120, 221, 
7, 27, 35, 64, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
25, 77, 99, 190, 50, 149, 191, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 115, 144, 255, 114, 51, 50, 255, 
181, 48, 42, 255, 185, 49, 43, 255, 
135, 49, 45, 255, 142, 52, 48, 255, 
159, 58, 54, 255, 91, 61, 64, 255, 
47, 124, 158, 255, 41, 124, 160, 251, 
29, 86, 110, 201, 14, 49, 63, 88, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
20, 66, 84, 100, 42, 126, 162, 245, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 142, 182, 255, 90, 67, 81, 255, 
221, 15, 6, 255, 254, 15, 3, 255, 
239, 17, 6, 255, 236, 18, 7, 255, 
221, 24, 14, 255, 84, 74, 88, 255, 
41, 120, 153, 255, 20, 63, 81, 185, 
14, 38, 52, 53, 0, 0, 0, 5, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
7, 35, 49, 36, 30, 92, 118, 194, 
48, 143, 185, 252, 50, 151, 194, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 151, 194, 255, 48, 141, 180, 255, 
77, 74, 90, 255, 194, 21, 15, 255, 
221, 16, 6, 255, 212, 19, 12, 255, 
96, 51, 59, 251, 31, 90, 116, 226, 
17, 52, 68, 150, 0, 0, 0, 5, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 2, 7, 35, 47, 64, 
31, 94, 121, 205, 50, 149, 192, 254, 
51, 152, 195, 255, 51, 152, 195, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
50, 149, 192, 255, 57, 100, 125, 255, 
83, 84, 103, 255, 58, 59, 73, 240, 
25, 75, 98, 168, 13, 39, 52, 78, 
0, 21, 21, 12, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 3, 
8, 35, 49, 57, 27, 84, 108, 203, 
43, 130, 166, 246, 51, 151, 194, 255, 
51, 152, 195, 255, 51, 152, 195, 255, 
49, 146, 188, 255, 35, 105, 134, 237, 
26, 78, 98, 196, 19, 59, 76, 90, 
0, 21, 31, 24, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 9, 
12, 36, 45, 85, 16, 50, 64, 142, 
18, 59, 75, 151, 18, 58, 75, 149, 
11, 39, 51, 115, 0, 0, 0, 22, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
0, 0, 0, 0, 0, 0, 0, 0, 
};

#endif
