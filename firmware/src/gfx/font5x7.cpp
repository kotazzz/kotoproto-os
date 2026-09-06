#include "koto/gfx/font5x7.hpp"

#include <cctype>

namespace koto {
namespace gfx {
namespace {

const char* kQuestion =
    "XXXXX"
    "X...X"
    "....X"
    "...X."
    "..X.."
    "....."
    "..X..";

}  // namespace

const char* glyph5x7(char ch) {
  const unsigned char uc = static_cast<unsigned char>(ch);
  if (uc >= 'a' && uc <= 'z') {
    ch = static_cast<char>(std::toupper(uc));
  }

  switch (ch) {
    case ' ':
      return "....."
             "....."
             "....."
             "....."
             "....."
             "....."
             ".....";
    case '!':
      return "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "....."
             "..X..";
    case '"':
      return ".X.X."
             ".X.X."
             "....."
             "....."
             "....."
             "....."
             ".....";
    case '#':
      return ".X.X."
             "XXXXX"
             ".X.X."
             ".X.X."
             "XXXXX"
             ".X.X."
             ".....";
    case '$':
      return "..X.."
             ".XXXX"
             "X.X.."
             ".XXX."
             "..X.X"
             "XXXX."
             "..X..";
    case '%':
      return "XX..X"
             "XX.X."
             "...X."
             "..X.."
             ".X..."
             "X.XX."
             "X..XX";
    case '&':
      return ".XX.."
             "X..X."
             "X.X.."
             ".X..."
             "X.X.X"
             "X..X."
             ".XX.X";
    case '\'':
      return "..X.."
             "..X.."
             "....."
             "....."
             "....."
             "....."
             ".....";
    case '(':
      return "...X."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "...X.";
    case ')':
      return ".X..."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             ".X...";
    case '*':
      return "....."
             "..X.."
             "X.X.X"
             ".XXX."
             "X.X.X"
             "..X.."
             ".....";
    case '+':
      return "....."
             "..X.."
             "..X.."
             "XXXXX"
             "..X.."
             "..X.."
             ".....";
    case ',':
      return "....."
             "....."
             "....."
             "....."
             "..X.."
             "..X.."
             ".X...";
    case '-':
      return "....."
             "....."
             "....."
             "XXXXX"
             "....."
             "....."
             ".....";
    case '.':
      return "....."
             "....."
             "....."
             "....."
             "....."
             "....."
             "..X..";
    case '/':
      return "....X"
             "...X."
             "..X.."
             ".X..."
             "X...."
             "....."
             ".....";
    case '0':
      return ".XXX."
             "X...X"
             "X..XX"
             "X.X.X"
             "XX..X"
             "X...X"
             ".XXX.";
    case '1':
      return "..X.."
             ".XX.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             ".XXX.";
    case '2':
      return ".XXX."
             "X...X"
             "....X"
             "...X."
             "..X.."
             ".X..."
             "XXXXX";
    case '3':
      return "XXXX."
             "....X"
             "....X"
             ".XXX."
             "....X"
             "....X"
             "XXXX.";
    case '4':
      return "...X."
             "..XX."
             ".X.X."
             "X..X."
             "XXXXX"
             "...X."
             "...X.";
    case '5':
      return "XXXXX"
             "X...."
             "XXXX."
             "....X"
             "....X"
             "X...X"
             ".XXX.";
    case '6':
      return ".XXX."
             "X...."
             "X...."
             "XXXX."
             "X...X"
             "X...X"
             ".XXX.";
    case '7':
      return "XXXXX"
             "....X"
             "...X."
             "..X.."
             ".X..."
             ".X..."
             ".X...";
    case '8':
      return ".XXX."
             "X...X"
             "X...X"
             ".XXX."
             "X...X"
             "X...X"
             ".XXX.";
    case '9':
      return ".XXX."
             "X...X"
             "X...X"
             ".XXXX"
             "....X"
             "....X"
             ".XXX.";
    case ':':
      return "....."
             "..X.."
             "....."
             "....."
             "....."
             "..X.."
             ".....";
    case ';':
      return "....."
             "..X.."
             "....."
             "....."
             "..X.."
             "..X.."
             ".X...";
    case '<':
      return "...X."
             "..X.."
             ".X..."
             "X...."
             ".X..."
             "..X.."
             "...X.";
    case '=':
      return "....."
             "....."
             "XXXXX"
             "....."
             "XXXXX"
             "....."
             ".....";
    case '>':
      return ".X..."
             "..X.."
             "...X."
             "....X"
             "...X."
             "..X.."
             ".X...";
    case '?':
      return kQuestion;
    case '@':
      return ".XXX."
             "X...X"
             "X.XXX"
             "X.X.X"
             "X.XXX"
             "X...."
             ".XXX.";
    case 'A':
      return ".XXX."
             "X...X"
             "X...X"
             "XXXXX"
             "X...X"
             "X...X"
             "X...X";
    case 'B':
      return "XXXX."
             "X...X"
             "X...X"
             "XXXX."
             "X...X"
             "X...X"
             "XXXX.";
    case 'C':
      return ".XXX."
             "X...X"
             "X...."
             "X...."
             "X...."
             "X...X"
             ".XXX.";
    case 'D':
      return "XXXX."
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "XXXX.";
    case 'E':
      return "XXXXX"
             "X...."
             "X...."
             "XXXX."
             "X...."
             "X...."
             "XXXXX";
    case 'F':
      return "XXXXX"
             "X...."
             "X...."
             "XXXX."
             "X...."
             "X...."
             "X....";
    case 'G':
      return ".XXX."
             "X...X"
             "X...."
             "X.XXX"
             "X...X"
             "X...X"
             ".XXX.";
    case 'H':
      return "X...X"
             "X...X"
             "X...X"
             "XXXXX"
             "X...X"
             "X...X"
             "X...X";
    case 'I':
      return ".XXX."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             ".XXX.";
    case 'J':
      return "..XXX"
             "...X."
             "...X."
             "...X."
             "...X."
             "X..X."
             ".XX..";
    case 'K':
      return "X...X"
             "X..X."
             "X.X.."
             "XX..."
             "X.X.."
             "X..X."
             "X...X";
    case 'L':
      return "X...."
             "X...."
             "X...."
             "X...."
             "X...."
             "X...."
             "XXXXX";
    case 'M':
      return "X...X"
             "XX.XX"
             "X.X.X"
             "X.X.X"
             "X...X"
             "X...X"
             "X...X";
    case 'N':
      return "X...X"
             "XX..X"
             "X.X.X"
             "X.X.X"
             "X..XX"
             "X...X"
             "X...X";
    case 'O':
      return ".XXX."
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             ".XXX.";
    case 'P':
      return "XXXX."
             "X...X"
             "X...X"
             "XXXX."
             "X...."
             "X...."
             "X....";
    case 'Q':
      return ".XXX."
             "X...X"
             "X...X"
             "X...X"
             "X.X.X"
             "X..X."
             ".XX.X";
    case 'R':
      return "XXXX."
             "X...X"
             "X...X"
             "XXXX."
             "X.X.."
             "X..X."
             "X...X";
    case 'S':
      return ".XXXX"
             "X...."
             "X...."
             ".XXX."
             "....X"
             "....X"
             "XXXX.";
    case 'T':
      return "XXXXX"
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X.."
             "..X..";
    case 'U':
      return "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             ".XXX.";
    case 'V':
      return "X...X"
             "X...X"
             "X...X"
             "X...X"
             "X...X"
             ".X.X."
             "..X..";
    case 'W':
      return "X...X"
             "X...X"
             "X...X"
             "X.X.X"
             "X.X.X"
             "XX.XX"
             "X...X";
    case 'X':
      return "X...X"
             "X...X"
             ".X.X."
             "..X.."
             ".X.X."
             "X...X"
             "X...X";
    case 'Y':
      return "X...X"
             "X...X"
             ".X.X."
             "..X.."
             "..X.."
             "..X.."
             "..X..";
    case 'Z':
      return "XXXXX"
             "....X"
             "...X."
             "..X.."
             ".X..."
             "X...."
             "XXXXX";
    case '[':
      return ".XXX."
             ".X..."
             ".X..."
             ".X..."
             ".X..."
             ".X..."
             ".XXX.";
    case '\\':
      return "X...."
             ".X..."
             "..X.."
             "...X."
             "....X"
             "....."
             ".....";
    case ']':
      return ".XXX."
             "...X."
             "...X."
             "...X."
             "...X."
             "...X."
             ".XXX.";
    case '^':
      return "..X.."
             ".X.X."
             "X...X"
             "....."
             "....."
             "....."
             ".....";
    case '_':
      return "....."
             "....."
             "....."
             "....."
             "....."
             "....."
             "XXXXX";
    default:
      return nullptr;
  }
}

}  // namespace gfx
}  // namespace koto
