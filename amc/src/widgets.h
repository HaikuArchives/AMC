#include <View.h>
#include <TextView.h>

class Slider : public BView {
public:
  Slider(BRect frame, const char *label, int, int minValue, int maxValue,
    uint32 resizingMode);
  ~Slider();
  void SetLimitLabels(const char *pMinLabel, const char *pMaxLabel);
  void SetValue(int value);
  void SetText(const char*,...);
  virtual void UpdateText();
  BMessage *theMessage, // sent when mouse up
           *modMessage; // message sent while dragging
  int value;

protected:
  // Hook functions.
  void SetOwnValue(int value);
  void AttachedToWindow();
  virtual void Draw(BRect updateRect);
  void MouseDown(BPoint where);
  void MouseUp(BPoint where);
  void MouseMoved(BPoint where, uint32, const BMessage*);
  virtual int valueForPoint(BPoint where)=0;
  virtual void calcBarFrame()=0;
  virtual BRect getThumbFrame()=0;
  virtual void drawText()=0;
  virtual BPoint getTextPos()=0;
  void createOffscreenView();
  virtual void grid();
  float valueToPosition(int value) const;
  int positionToValue(float position,int min,int max) const;

  // Data members.
  const int minValue,    // limits of the slider's value range
            maxValue;
  bool tracking;  // mouse tracking data
  BPoint trackingOffset,
         textXY;
  char *minLabel,    // limit labels
       *maxLabel,
       buf[20];
  BRect textRect;    // text rectangle  
  BBitmap *offscreenBits;  // offscreen buffer
  BView *offscreenView;
  rgb_color viewColor;
  font_height fontHeight;
  BRect barFrame;
  const char* theLabel;
  static const float thumbWidth = 8,
               barThickness = 5;
};

struct HSlider: public Slider {
  HSlider(BRect frame, const char *label,
    int msg, int minValue, int maxValue,
    uint32 resizingMode = B_FOLLOW_LEFT);
private:
  int valueForPoint(BPoint where);
  void calcBarFrame();
  BRect getThumbFrame();
  void drawText();
  BPoint getTextPos();
  void grid();
};

struct VSlider: public Slider {
  VSlider(BRect frame, const char *label,
    int msg, int minValue, int maxValue,
    uint32 resizingMode = B_FOLLOW_LEFT);
private:
  int valueForPoint(BPoint where);
  void calcBarFrame();
  BRect getThumbFrame();
  void drawText();
  BPoint getTextPos();
  void grid();
};

struct HVSlider: public Slider {
  HVSlider(BRect frame, const char *label, int msg, int msgY, float h_inset,
    int minValX, int maxValX, int minValY, int maxValY,
    uint32 resizingMode = B_FOLLOW_LEFT);
  void SetLimitLabels(const char*, const char*,const char*, const char*);
  void SetValueY(int val);
  void SetTextY(const char*,...);
  void UpdateText();
  BMessage *theMessageY, // sent when mouse up
           *modMessageY; // message sent while dragging
  int valueY;
private:
  void SetOwnValueY(int val);
  void MouseMoved(BPoint where, uint32, const BMessage*);
  void MouseUp(BPoint where);
  int valueForPoint(BPoint where);
  int valueForPointY(BPoint where);
  int valXForPoint(BPoint where);
  int valYForPoint(BPoint where);
  void calcBarFrame();
  BRect getThumbFrame();
  float valueToPositionY(int value) const;
  void drawText();
  BPoint getTextPos();
  BPoint getTextPosY();
  void grid();
  float hor_inset;
  int minValueY,maxValueY;
  char *minLabelY,
       *maxLabelY,
       bufY[20];
  BRect textRectY;    // vert text rectangle  
};

struct UpdText {  // updatable text
  char buf[50];
  BView *theView;
  BPoint top;
  rgb_color textColor;
  font_height fontHeight;
  UpdText(BView *vp,BPoint p);
  void write(const char *form,...);
};

struct UpdTextBg:UpdText {
  rgb_color bgColor;
  UpdTextBg(BView *vp,BPoint p);
  void write(const char *form,...);
};

class RadioButton: public BView {
public:
  RadioButton(BRect, struct RButCtrl*, const char* label, int32 mess_nr);
  ~RadioButton();
  BMessage *theMessage,
           *NoneMessage;
  rgb_color textColor;
  void SetValue(bool);
  float dx(),dy();
  void reLabel(const char*);
  void Remove();
  friend class RButCtrl;
protected:
  void drawArrow();
  void drawLabel();
  void AttachedToWindow();
  void MouseDown(BPoint where);
  void Draw(BRect);
  BRect area;
  struct RButCtrl* theControl;
  bool theValue;
  const char* theLabel;
};

class RButCtrl {
public:
  enum Mode { Always1, AtMost1L, AtMost1K };
  float min_width;
  RadioButton* AddButton(BView* view, BPoint top, char* label, int32); 
  void reset();
  friend class RadioButton;
protected:
  Mode mode;
  font_height fontHeight;
  float height;
  int theMess;
  const uint32 theResMode;
  RadioButton *act_button;
  RButCtrl(int mes, uint32 resizingMode, Mode);
};

class RButtonArrowCtrl: public RButCtrl {
public:
  RButtonArrowCtrl(int msg, uint32 resizingMode = B_FOLLOW_LEFT);
};

class RButtonAM1LampCtrl: public RButCtrl {
public:
  RButtonAM1LampCtrl(int, uint32 resizingMode = B_FOLLOW_LEFT);
};

class RButtonAM1KnobCtrl: public RButCtrl {
public:
  RButtonAM1KnobCtrl(int, uint32 resizingMode = B_FOLLOW_LEFT);
};

class CheckBox: public BView {
public:
  CheckBox(BPoint, const char *label, int msg,
    uint32 resizingMode = B_FOLLOW_LEFT);
  void SetValue(bool);
  BMessage *theMessage;
  bool value;
protected:
  BRect area;
  const char *theLabel;
  font_height fontHeight;
  void AttachedToWindow();
  void MouseDown(BPoint where);
  void Draw(BRect);
};

class UpDown: public BView {
public:
  UpDown(BPoint, const char *label, int msg,
    uint32 resizingMode = B_FOLLOW_LEFT);
  BMessage *theMessage;
  int8 value; // -1,0,+1
protected:
  BRect area_down,area_up;
  const char *theLabel;
  font_height fontHeight;
  void AttachedToWindow();
  void MouseDown(BPoint where);
  void MouseUp(BPoint where);
  void Draw(BRect);
};

class Button: public BView {
public:
  Button(BRect, const char *label, int mes,
    uint32 resizingMode = B_FOLLOW_LEFT);
  rgb_color textColor;
  BMessage *theMessage;
  virtual void drawLabel();
  float dx();
protected:
  BRect area;
  const char *theLabel;
  font_height fontHeight;
  void MouseDown(BPoint where);
  void MouseUp(BPoint where);
  void Draw(BRect);
};

class TextControl: public BTextView {
public:
  BMessage *theMessage;
  TextControl(BRect frame,int mes,uint32);
  void reset();
protected:
  BRect area;
  float xoff;
  font_height fontHeight;
  void InsertText(const char *text,int32 length,int32 offset,const text_run_array *runs);
  void KeyDown(const char *bytes,int32 n);
  void FrameResized(float w,float h);
  void Draw(BRect);
};

class RotateChoice:public BView {
public:
  virtual void draw(int) = 0;
  BMessage *theMessage;
  int value;
  RotateChoice(BRect,int mes,int, uint32 resizingMode = B_FOLLOW_LEFT);
private:
  const int dim;
  void Draw(BRect);
  void MouseDown(BPoint where);
};

//void sunk(BView* v,BRect r);
//void raised(BView* v,BRect r);
//void knob(BView* v,BRect r,bool val);
//void arrow(BView* v,BRect r,bool val);
//void lamp(BView* v,BRect r,bool val);
BWindow *call_alert(BRect,const char* text,int *nr=0);
