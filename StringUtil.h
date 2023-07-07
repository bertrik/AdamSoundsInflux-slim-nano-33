#ifndef _STRING_UTIL_H_
#define _STRING_UTIL_H_

// Convert a single hex digit character to its integer value
unsigned char h2int(char c) {
  if (c >= '0' && c <= '9') {
    return ((unsigned char)c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return ((unsigned char)c - 'a' + 10);
  }
  if (c >= 'A' && c <= 'F') {
    return ((unsigned char)c - 'A' + 10);
  }
  return (0);
}

// Based on https://code.google.com/p/avr-netino/
String urldecode(String input) { 
  char c;
  String ret = "";

  for (byte t = 0; t < input.length(); t++) {
    c = input[t];
    if (c == '+') c = ' ';
    if (c == '%') {
      t++;
      c = input[t];
      t++;
      c = (h2int(c) << 4) | h2int(input[t]);
    }
    ret.concat(c);
  }
  return ret;
}

size_t copyString(const String& input, char *dst, size_t max_size)
{
  size_t s = input.length();
  if(s >= max_size - 1) {
    s = max_size - 1;
  }
  strncpy(dst, input.c_str(), s);
  dst[s] = '\0';
  return s;
}


String escapeJsonString(const String& input) {
  String output;
  for (size_t i=0;i<input.length();i++) {
      char c = input.charAt(i);
      switch (c) {
          case '\\': output.concat("\\\\"); break;
          case '"' : output.concat("\\\""); break;
          case '/' : output.concat("\\/");  break;
          case '\b': output.concat("\\b");  break;
          case '\f': output.concat("\\f");  break;
          case '\n': output.concat("\\n");  break;
          case '\r': output.concat("\\r");  break;
          case '\t': output.concat("\\t");  break;
          default: output.concat(c); break;
      }
  }
  return output;
}

int doubleLength(double d) {
    return snprintf(NULL, 0, "%.2f", d);
}

int intLength(int i) {
    return snprintf(NULL, 0, "%i", i);
}

#endif
