struct Bitmaps {
  BBitmap *sharp_bm,
          *flat_bm,
          *dum_bm;
  Bitmaps();
  BBitmap *get(int bm);
};
  
enum {
  eFlat=1, eSharp  // signs
};

extern const uint8
  sq_cursor_data[],
  pnt_cursor_data[];
