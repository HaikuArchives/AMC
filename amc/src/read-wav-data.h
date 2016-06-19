const int raw_max=50;

struct RawData {
  int size;
  int16 *buf;
  char *file_name;
};

extern RawData RAW[];
void alert(const char *s,...);
bool fill_raw_data(const char *dir,const int sbm);
extern bool raw_data_okay;
