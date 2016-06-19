const int str_max=60;

struct Str {
  char ch,      // last read character
       cmt_ch,  // after this char: comment
       s[str_max+1];  // 1 extra for closing 0
  Str();
  Str(const char*);
  void cpy(const char*);
  void cat(const char*);
  void rword(FILE* in,const char *delim);
  void strtok(const char*,const char *delim,int& pos);
  char *tos(int); 
  void new_ext(const char *ext);
  char *get_dir(); 
  char *strip_dir(); 
  char *get_ext(); 
  bool operator==(const char*);
}; 
