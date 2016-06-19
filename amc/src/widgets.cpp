#include <Bitmap.h>
#include <Window.h>
#include <Message.h>
#include <TextView.h>
#include <printf.h>
#include <stdarg.h>
#include "widgets.h"

extern rgb_color cLightGrey,
                 cGrey,
                 cDarkGrey,
                 cBlack,
                 cWhite,
                 cDarkBlue,
                 cRed,
                 cPink;

extern BFont *appFont;
extern const int nop;

float fmax(float a,float b) { return a>b ? a : b; }
float fmin(float a,float b) { return a<b ? a : b; }

void raised(BView* v,BRect r) {
  v->SetPenSize(1);
  for (int n=0;n<2;++n) {
    v->SetHighColor(cDarkGrey);
    v->StrokeLine(BPoint(r.left,r.bottom),BPoint(r.right,r.bottom));
    v->StrokeLine(BPoint(r.right,r.top));
    v->SetHighColor(n==0?cGrey:cWhite);
    v->StrokeLine(BPoint(r.left,r.bottom),BPoint(r.left,r.top));
    v->StrokeLine(BPoint(r.right,r.top));
    r.InsetBy(1,1);
  }
}

void sunk(BView* v,BRect r) {
  v->SetPenSize(1);
  for (int n=0;n<2;++n) {
    v->SetHighColor(cLightGrey);
    v->StrokeLine(BPoint(r.left,r.bottom),BPoint(r.right,r.bottom));
    v->StrokeLine(BPoint(r.right,r.top));
    v->SetHighColor(cDarkGrey);
    v->StrokeLine(BPoint(r.left,r.bottom),BPoint(r.left,r.top));
    v->StrokeLine(BPoint(r.right,r.top));
    r.InsetBy(1,1);
  }
}

void knob(BView* v,BRect r,bool val) {
  v->SetPenSize(1);
  if (val) sunk(v,r);
  else raised(v,r);
  r.InsetBy(2,2); 
  v->SetHighColor(cGrey); 
  v->FillRect(r); 
}

void arrow(BView* v,BRect r,bool val) {
  float y = (r.bottom + r.top) * 0.5 + 1,
        x = r.right;
  BPoint plist[] = { BPoint(x-4,y),BPoint(x-5,y+4),BPoint(x,y),BPoint(x-5,y-4) };
  v->SetHighColor(val?cRed:cWhite);
  v->FillPolygon(plist,4);
  v->SetPenSize(3);
  v->StrokeLine(BPoint(r.left,y),BPoint(x-4,y));
}

void ud_arrow(BView* v,BRect r,bool up,bool val) {
  float x = (r.left + r.right) * 0.5;
  v->SetHighColor(val?cWhite:cDarkGrey);
  if (up)
    v->FillTriangle(BPoint(x,0),BPoint(x-4,4),BPoint(x+4,4));
  else
    v->FillTriangle(BPoint(x,r.bottom),BPoint(x-4,r.bottom-4),BPoint(x+4,r.bottom-4));
}

void check(BView* v,BRect r,bool val) {
  static float y = (r.bottom - r.top) * 0.5 + 6,
               x = (r.right - r.left) * 0.5;
  static BPoint plist[] = { BPoint(r.left,y-6),BPoint(x,y),BPoint(r.right,y-9) };
  v->SetHighColor(val?cRed:cWhite);
  v->SetPenSize(2);
  v->StrokePolygon(plist,3,false);
}

void lamp(BView* v,BRect r,bool val) {
  sunk(v,r);
  v->SetHighColor(val ? cRed : cWhite); 
  r.InsetBy(2,2);
  v->FillRect(r); 
}

Slider::Slider(
    BRect frame, const char *label,
    int msg, int minVal, int maxVal,
    uint32 resizingMode):
  BView(frame, 0, resizingMode, B_WILL_DRAW),
    theMessage(msg ? new BMessage(msg) : 0),
    value(0),
    minValue(minVal), maxValue(maxVal),
    modMessage(0),
    tracking(false),
    trackingOffset(B_ORIGIN),
    textXY(B_ORIGIN),
    minLabel(0),
    maxLabel(0),
    textRect(BRect(0,0,0,0)),
    offscreenBits(0),
    offscreenView(0),
    viewColor(cWhite),
    theLabel(label) {
  if (minValue >= maxValue)
    debugger("Slider: Minimum value >= maximum");

  buf[0]=0;
  SetFont(appFont,B_FONT_SIZE);
  appFont->GetHeight(&fontHeight);
}

Slider::~Slider() {
  delete modMessage; modMessage=0;
  delete offscreenBits; offscreenBits=0; offscreenView=0; // this also deletes the attached view
}

HSlider::HSlider(
    BRect frame, const char *label,
    int msg, int minVal, int maxVal,
    uint32 resizingMode):
    Slider(frame,label,msg,minVal,maxVal,resizingMode) {
  const float lineHeight = fontHeight.ascent + fontHeight.descent,
              height = 2 * lineHeight + Slider::barThickness + 10;
  ResizeTo(Bounds().Width(),height);
  calcBarFrame();
}

VSlider::VSlider(
    BRect frame, const char *label,
    int msg, int minVal, int maxVal,
    uint32 resizingMode):
    Slider(frame,label,msg,minVal,maxVal,resizingMode) {
  calcBarFrame();
}

HVSlider::HVSlider(
    BRect frame, const char *label, int msg, int msgY, float h_inset,
    int minValx, int maxValx, int minValy, int maxValy,
    uint32 resizingMode
  ):
    Slider(frame,label,msg,minValx,maxValx,resizingMode),
    theMessageY(msgY ? new BMessage(msgY) : 0),
    modMessageY(0),
    valueY(0),
    hor_inset(h_inset),
    minValueY(minValy),maxValueY(maxValy),
    minLabelY(0),maxLabelY(0),
    textRectY(BRect(0,0,0,0)) {
  bufY[0]=0;
  calcBarFrame();
}

void Slider::AttachedToWindow() {
  if (!offscreenView) {
    viewColor=Parent()->ViewColor();
    SetLowColor(viewColor);
    // Set view color to transparent to avoid redraw flicker.
    SetViewColor(B_TRANSPARENT_COLOR);
    createOffscreenView();
  }
}

void Slider::grid() { }

void HSlider::grid() {
  float dist=barFrame.Width()/(maxValue-minValue);
  if (dist<5) return; 
  offscreenView->SetHighColor(cWhite);
  for (int n=minValue+1;n<maxValue;++n) {
    float x=(n-minValue)*dist+barFrame.left;
    offscreenView->StrokeLine(BPoint(x,barFrame.top+2),BPoint(x,barFrame.bottom-2));
  }
}

void VSlider::grid() {
  float dist=barFrame.Height()/(maxValue-minValue);
  if (dist<5) return; 
  offscreenView->SetHighColor(cWhite);
  for (int n=minValue+1;n<maxValue;++n) {
    float y=(n-minValue)*dist+barFrame.top;
    offscreenView->StrokeLine(BPoint(barFrame.left+2,y),BPoint(barFrame.right-2,y));
  }
}

void HVSlider::grid() {
  float dist=barFrame.Width()/(maxValue-minValue),
        distY=barFrame.Height()/(maxValueY-minValueY);
  offscreenView->SetHighColor(cWhite);
  int n;
  for (n=minValue+1;n<maxValue;++n) {
    float x=(n-minValue)*dist+barFrame.left;
    offscreenView->StrokeLine(BPoint(x,barFrame.top+2),BPoint(x,barFrame.bottom-2));
  }
  for (n=minValueY+1;n<maxValueY;++n) {
    float y=(n-minValueY)*distY+barFrame.top;
    offscreenView->StrokeLine(BPoint(barFrame.left+2,y),BPoint(barFrame.right-2,y));
  }
}

void Slider::Draw(BRect updateRect) {
  // Draw into offscreen buffer, then copy to screen.
  if (offscreenBits->Lock()) {
    // Erase offscreen view.
    offscreenView->SetHighColor(viewColor);
    offscreenView->FillRect(offscreenView->Bounds());
    // Draw individual components.
    knob(offscreenView,barFrame,true);
    grid();
    knob(offscreenView,getThumbFrame(),false);
    drawText();
    offscreenView->Sync();
    offscreenBits->Unlock();
  }

  DrawBitmap(offscreenBits,BPoint(0,0));
  UpdateText();
}

void Slider::MouseDown(BPoint where) {
  const BRect frame = getThumbFrame();
  
  if (frame.Contains(where)) {
    tracking = true;
    trackingOffset = BPoint(frame.right - where.x,frame.top - where.y);
    // B_POINTER_EVENTS: all pointer events
    SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS|B_NO_POINTER_HISTORY);
  }
}

void Slider::MouseMoved(BPoint where, uint32 location, const BMessage *) { 
  if (tracking && location != B_OUTSIDE_VIEW) {
    SetOwnValue(valueForPoint(trackingOffset + where));
  }
}

void HVSlider::MouseMoved(BPoint where, uint32 location, const BMessage *)
{ 
  if (tracking && location != B_OUTSIDE_VIEW) {
    SetOwnValue(valueForPoint(trackingOffset + where));
    SetOwnValueY(valueForPointY(trackingOffset + where));
  }
}

void Slider::MouseUp(BPoint) {
  if (tracking) {
    tracking = false;
    if (theMessage) Window()->PostMessage(theMessage);
  }
}

void HVSlider::MouseUp(BPoint) {
  if (tracking) {
    tracking = false;
    if (theMessage) Window()->PostMessage(theMessage);
    if (theMessageY) Window()->PostMessage(theMessageY);
  }
}

void Slider::SetValue(int val)
{ 
  if (val < minValue) val = minValue; else if (val > maxValue) val = maxValue;
  if (val != value) {
    value = val;
    Invalidate();
  }
}

void Slider::SetOwnValue(int val)
{
  if (val != value) {
    value = val;
    Invalidate();
    if (modMessage) Window()->PostMessage(modMessage);
  }
}

void HVSlider::SetValueY(int val)
{
  if (val < minValueY) val = minValueY; else if (val > maxValueY) val = maxValueY;
  if (val != valueY) {
    valueY = val;
    Invalidate();
  }
}

void HVSlider::SetOwnValueY(int val)
{
  if (val != valueY) {
    valueY = val;
    Invalidate();
    if (modMessageY) Window()->PostMessage(modMessageY);
  }
}

void Slider::SetLimitLabels(const char *minLab, const char *maxLab)
{
  minLabel = const_cast<char*>(minLab);
  maxLabel = const_cast<char*>(maxLab);
}

void HVSlider::SetLimitLabels(
  const char *minLx, const char *maxLx, const char *minLy, const char *maxLy)
{
  minLabel = const_cast<char*>(minLx);
  maxLabel = const_cast<char*>(maxLx);
  minLabelY = const_cast<char*>(minLy);
  maxLabelY = const_cast<char*>(maxLy);
}

int HSlider::valueForPoint(BPoint where) {
  const float pos = (where.x - barFrame.left) / barFrame.Width();
  return positionToValue(pos,minValue,maxValue);
}

int VSlider::valueForPoint(BPoint where) {
  const float pos = (barFrame.bottom - where.y) / barFrame.Height();
  return positionToValue(pos,minValue,maxValue);
}

int HVSlider::valueForPoint(BPoint where) {
  const float pos = (where.x - barFrame.left) / barFrame.Width();
  return positionToValue(pos,minValue,maxValue);
}

int HVSlider::valueForPointY(BPoint where) {
  const float pos = (barFrame.bottom - where.y) / barFrame.Height();
  return positionToValue(pos, minValueY, maxValueY);
}

void HSlider::calcBarFrame() {
  const float lineHeight = fontHeight.ascent + fontHeight.descent;
  barFrame = Bounds();
  barFrame.InsetBy(4, 0);
  barFrame.top += lineHeight + 6;
  barFrame.bottom = barFrame.top + Slider::barThickness;
  textXY.y = barFrame.bottom + fontHeight.ascent + 4;
}

void VSlider::calcBarFrame() {
  const float lineHeight = fontHeight.ascent + fontHeight.descent;
  barFrame = Bounds();
  barFrame.top += lineHeight + 6;
  barFrame.left += 5;
  barFrame.right = barFrame.left  + Slider::barThickness;
  barFrame.bottom -= 4;
  textXY.x = barFrame.right + 8;
}

void HVSlider::calcBarFrame() {
  const float lineHeight = fontHeight.ascent + fontHeight.descent;
  barFrame = Bounds();
  barFrame.left += 4;
  barFrame.right -= hor_inset;
  barFrame.top += lineHeight + 6;
  barFrame.bottom -= lineHeight + 4;
  textXY.x = barFrame.right + 8;
  textXY.y = barFrame.bottom + fontHeight.ascent + 4;
}

BRect HSlider::getThumbFrame() {
  BRect frame = barFrame;
  frame.left += frame.Width() * valueToPosition(value) - Slider::thumbWidth/2;
  frame.right = frame.left + Slider::thumbWidth;
  frame.top -= 4;
  frame.bottom += 4;
  return frame;
}

BRect VSlider::getThumbFrame() {
  BRect frame = barFrame;
  frame.right += 4.;
  frame.left -= 4.;
  frame.bottom -= frame.Height() * valueToPosition(value) - Slider::thumbWidth/2;
  frame.top = frame.bottom - Slider::thumbWidth;
  return frame;
}

BRect HVSlider::getThumbFrame() {
  BRect frame = barFrame;
  frame.left += frame.Width() * valueToPosition(value) - Slider::thumbWidth/2;
  frame.right = frame.left + Slider::thumbWidth;
  frame.bottom -= frame.Height() * valueToPositionY(valueY) - Slider::thumbWidth/2;
  frame.top = frame.bottom - Slider::thumbWidth;
  return frame;
}

void HSlider::drawText() {
  offscreenView->SetHighColor(cBlack);
  offscreenView->DrawString(theLabel, BPoint(Bounds().left,Bounds().top + fontHeight.ascent));
  if (minLabel)
    offscreenView->DrawString(minLabel,BPoint(Bounds().left,textXY.y));
  if (maxLabel)
    offscreenView->DrawString(maxLabel,
      BPoint(Bounds().right - offscreenView->StringWidth(maxLabel),textXY.y));
}

void VSlider::drawText() {
  const BRect r=Bounds();
  offscreenView->SetHighColor(cBlack);
  offscreenView->DrawString(theLabel, BPoint(r.left,r.top + fontHeight.ascent));
  if (minLabel)
    offscreenView->DrawString(minLabel, BPoint(textXY.x, barFrame.bottom + fontHeight.descent));
  if (maxLabel)
    offscreenView->DrawString(maxLabel, BPoint(textXY.x, barFrame.top + fontHeight.descent));
}

void HVSlider::drawText() {
  offscreenView->SetHighColor(cBlack);
  const BRect r = Bounds();
  offscreenView->DrawString(theLabel, BPoint(r.left,r.top + fontHeight.ascent));
  if (minLabel)
    offscreenView->DrawString(minLabel, BPoint(r.left,textXY.y));
  if (maxLabel)
    offscreenView->DrawString(maxLabel,
      BPoint(r.right - offscreenView->StringWidth(maxLabel) - hor_inset + 4, textXY.y));
  if (minLabelY)
    offscreenView->DrawString(minLabelY, BPoint(textXY.x,barFrame.bottom + fontHeight.descent));
  if (maxLabelY)
    offscreenView->DrawString(maxLabelY, BPoint(textXY.x,barFrame.top + fontHeight.descent));
}

void Slider::SetText(const char* form,...) {
  SetHighColor(viewColor);   // erase old text
  FillRect(textRect);
  va_list ap;
  va_start(ap,form);
  vsprintf(buf,form,ap);
  va_end(ap);
  BPoint textPos(getTextPos());
  textRect=BRect(textPos.x,textPos.y - fontHeight.ascent,
    textPos.x+StringWidth(buf),textPos.y + fontHeight.descent);
}

void HVSlider::SetTextY(const char* form,...) {
  SetHighColor(viewColor);   // erase old text
  FillRect(textRectY);
  va_list ap;
  va_start(ap,form);
  vsprintf(bufY,form,ap);
  va_end(ap);
  BPoint textPos(getTextPosY());
  textRectY=BRect(textPos.x,textPos.y - fontHeight.ascent,
    textPos.x+StringWidth(buf),textPos.y + fontHeight.descent);
}

BPoint HSlider::getTextPos() {
  const BRect r = Bounds();
  return BPoint((r.right - r.left - StringWidth(buf))/2, textXY.y);
}

BPoint VSlider::getTextPos() {
  return BPoint(textXY.x, (barFrame.bottom + barFrame.top)/2 + fontHeight.descent);
}

BPoint HVSlider::getTextPos() {
  const BRect r = Bounds();
  return BPoint((r.right - hor_inset - r.left - StringWidth(buf))/2, textXY.y);
}

BPoint HVSlider::getTextPosY() {
  return BPoint(textXY.x, (barFrame.bottom + barFrame.top)/2 + fontHeight.descent);
}

void Slider::UpdateText() {
  SetHighColor(cDarkBlue);
  DrawString(buf, BPoint(textRect.left,textRect.top + fontHeight.ascent));
}

void HVSlider::UpdateText() {
  SetHighColor(cDarkBlue);
  DrawString(buf, BPoint(textRect.left,textRect.top + fontHeight.ascent));
  DrawString(bufY, BPoint(textRectY.left,textRectY.top + fontHeight.ascent));
}

float Slider::valueToPosition(int val) const {
  return static_cast<float>(val - minValue) / static_cast<float>(maxValue - minValue);
}

float HVSlider::valueToPositionY(int val) const {
  return static_cast<float>(val - minValueY) / static_cast<float>(maxValueY - minValueY);
}

int Slider::positionToValue(float position,int min,int max) const
{
  int val = min + static_cast<int>(position * (max - min));
  if (val < min) val = min;
  if (val > max) val = max;
  return val;
}

void Slider::createOffscreenView()
{
  if (offscreenBits || offscreenView) {
    debugger("Slider: offscreen 2 times created");
  }
  offscreenBits = new BBitmap(Bounds(), B_RGB32, true);
  offscreenView = new BView(Bounds(), 0, B_FOLLOW_NONE, B_WILL_DRAW);
  offscreenView->SetFont(appFont,B_FONT_SIZE);

  offscreenBits->AddChild(offscreenView);
}

RadioButton::RadioButton(BRect frame, RButCtrl* ctrl, const char* label, int32 mess_nr):
    BView(frame, 0, ctrl->theResMode, B_WILL_DRAW),
    textColor(cBlack),
    theControl(ctrl),
    theValue(false),
    theMessage(new BMessage(ctrl->theMess)),
    NoneMessage(new BMessage(ctrl->theMess)),
    theLabel(label) {
  switch (ctrl->mode) {
    case RButCtrl::AtMost1K:
      area=Bounds();
      break;
    default: 
      area.Set(0,0,10,ctrl->height);
  }
  SetFont(appFont,B_FONT_SIZE);
  theMessage->AddInt32("",mess_nr);
  int32 nop32=nop;
  NoneMessage->AddInt32("",nop32);
}

RadioButton::~RadioButton() {
  if (this==theControl->act_button) theControl->act_button=0;
  delete theMessage;
  delete NoneMessage;
}

void RadioButton::Remove() {
  if (theControl->act_button==this) theControl->act_button=0;
  RemoveSelf();
  delete(this);
}

void RadioButton::SetValue(bool v) {
  if (v) {
    RadioButton *rb=theControl->act_button;
    if (rb) rb->SetValue(false); 
    theControl->act_button=this;
  }
  theValue=v;
  switch (theControl->mode) {
    case RButCtrl::AtMost1K: Invalidate(); break;
    default: drawArrow();
  }
}

float RadioButton::dx() { return area.right; }

float RadioButton::dy() { return area.bottom; }

void RadioButton::MouseDown(BPoint where) {
  if (area.Contains(where)) {
    RadioButton* rb;
    switch (theControl->mode) {
      case RButCtrl::Always1:
        SetValue(true);
        Window()->PostMessage(theMessage);
        break;
      case RButCtrl::AtMost1L:
      case RButCtrl::AtMost1K:
        if (theValue) {
          SetValue(false);
          Window()->PostMessage(NoneMessage);
        }
        else {
          SetValue(true);
          Window()->PostMessage(theMessage);
        }
        break;
    }
  }
}

void RadioButton::drawArrow() {
  switch (theControl->mode) {
    case RButCtrl::Always1: arrow(this,area,theValue); break;
    case RButCtrl::AtMost1L: lamp(this,area,theValue); break;
    case RButCtrl::AtMost1K: knob(this,area,theValue); break;
  }
}

void RadioButton::Draw(BRect rect) {
  drawArrow();
  SetHighColor(textColor);
  drawLabel();
}

void RadioButton::drawLabel() {
  float y=theControl->fontHeight.ascent;
  switch (theControl->mode) {
    case RButCtrl::AtMost1K:
      SetLowColor(cGrey);
      DrawString(theLabel,BPoint(4,y+1));
      break;
    default: DrawString(theLabel,BPoint(15,y));
  }
}

void RadioButton::reLabel(const char* txt) {
  SetHighColor(Parent()->ViewColor());
  drawLabel(); // erase old label
  //printf("lab=%s txt=%s\n",theLabel,txt);
  theLabel=txt;
  float wid;
  switch (theControl->mode) {
    case RButCtrl::AtMost1K:
      break;
    default:
      wid=StringWidth(theLabel)+15;
      ResizeTo(wid,theControl->height);
      break;
  }
  SetHighColor(textColor);
  drawLabel();
}

RButCtrl::RButCtrl(int mes, uint32 resizingMode, Mode m):
    mode(m),
    min_width(0.),
    theMess(mes),
    theResMode(resizingMode),
    act_button(0) {
  appFont->GetHeight(&fontHeight);
  height=min_width=fontHeight.ascent + fontHeight.descent;
}

RButtonArrowCtrl::RButtonArrowCtrl(int mes, uint32 resizingMode):
  RButCtrl(mes, resizingMode, RButCtrl::Always1) {
}

RButtonAM1LampCtrl::RButtonAM1LampCtrl(int mes, uint32 resizingMode):
   RButCtrl(mes,resizingMode,RButCtrl::AtMost1L) {
}

RButtonAM1KnobCtrl::RButtonAM1KnobCtrl(int mes, uint32 resizingMode):
   RButCtrl(mes,resizingMode,RButCtrl::AtMost1K) {
}

void RadioButton::AttachedToWindow() {
  SetViewColor(Parent()->ViewColor());
  SetLowColor(Parent()->ViewColor());
}

void RButCtrl::reset() {
  if (act_button) {
    act_button->SetValue(false);
    act_button=0;
  }
}

RadioButton* RButCtrl::AddButton(BView* view, BPoint top, char* label, int32 mesn) {
  RadioButton* rb;
  float wid;
  switch (mode) {
    case AtMost1K:
      wid=fmax(view->StringWidth(label)+6,min_width+4); // square at least
      rb=new RadioButton(BRect(top.x,top.y,top.x+wid,top.y+height+4),
                         this,label,mesn);
      break;
    default: // Always1, AtMost1L
      rb=new RadioButton(BRect(top.x,top.y,top.x+15+view->StringWidth(label),top.y+height),
                         this,label,mesn);
      break;
  }
  rb->theControl=this;
  view->AddChild(rb);
  return rb;
}

CheckBox::CheckBox(BPoint top, const char* label, int mes,
                   uint32 resizingMode):
    BView(BRect(top.x,top.y,top.x,top.y), 0, resizingMode, B_WILL_DRAW),
    theMessage(mes ? new BMessage(mes) : 0),
    value(false),
    theLabel(label) {
  appFont->GetHeight(&fontHeight);
  SetFont(appFont,B_FONT_SIZE);
  float height=fontHeight.ascent + fontHeight.descent;
  ResizeTo(12 + StringWidth(label),height);
  area.Set(0,0,8,height);
}

void CheckBox::SetValue(bool val) {
  value=val;
  check(this,area,value);
}

void CheckBox::AttachedToWindow() {
  SetViewColor(Parent()->ViewColor());
  SetLowColor(Parent()->ViewColor());
}

void CheckBox::MouseDown(BPoint where) {
  if (area.Contains(where)) {
    SetValue(!value);
    if (theMessage) Window()->PostMessage(theMessage);
  }
}

void CheckBox::Draw(BRect) {
  check(this,area,value);
  SetHighColor(cBlack);
  DrawString(theLabel,BPoint(12,fontHeight.ascent));
}

UpDown::UpDown(BPoint top, const char* label, int mes, uint32 resizingMode):
    BView(BRect(top.x,top.y,0,0), 0, resizingMode, B_WILL_DRAW),
    theMessage(mes ? new BMessage(mes) : 0),
    theLabel(label),
    value(0) {
  appFont->GetHeight(&fontHeight);
  float height=fmax(fontHeight.ascent + fontHeight.descent,12);
  SetFont(appFont,B_FONT_SIZE);
  ResizeTo(StringWidth(label)+14,height);
  area_up.Set(0,0,10,height/2);
  area_down.Set(0,height/2,10,height);
}

void UpDown::MouseDown(BPoint where) {
  SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS|B_NO_POINTER_HISTORY);
  if (area_down.Contains(where)) {
    value=-1;
    ud_arrow(this,area_down,false,true);
    if (theMessage) Window()->PostMessage(theMessage);
  }
  else if (area_up.Contains(where)) {
    value=1;
    ud_arrow(this,area_up,true,true);
    if (theMessage) Window()->PostMessage(theMessage);
  }
}

void UpDown::MouseUp(BPoint where) {
  ud_arrow(this,area_down,false,false);
  ud_arrow(this,area_up,true,false);
}

void UpDown::AttachedToWindow() {
  SetViewColor(Parent()->ViewColor());
  SetLowColor(Parent()->ViewColor());
}

void UpDown::Draw(BRect rect) {
  ud_arrow(this,area_down,false,false);
  ud_arrow(this,area_up,true,false);
  SetHighColor(cBlack);
  DrawString(theLabel,BPoint(14,fontHeight.ascent));
}

Button::Button(BRect rect, const char* label, int mes, uint32 resizingMode):
    BView(rect, 0, resizingMode, B_WILL_DRAW),
    textColor(cBlack),
    theLabel(label),
    theMessage(mes ? new BMessage(mes) : 0) {
  appFont->GetHeight(&fontHeight);
  float height=fontHeight.ascent + fontHeight.descent;
  SetFont(appFont,B_FONT_SIZE);
  ResizeTo(fmax(rect.Width(),StringWidth(label)+6),height+4);
  area=Bounds();
  SetLowColor(cGrey);
}

void Button::MouseDown(BPoint where) {
  SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS|B_NO_POINTER_HISTORY);
  knob(this,area,true);
  SetHighColor(textColor);
  drawLabel();
  if (theMessage) Window()->PostMessage(theMessage);
}

void Button::MouseUp(BPoint where) {
  knob(this,area,false);
  SetHighColor(textColor);
  drawLabel();
}

void Button::drawLabel() {
  SetHighColor(textColor);
  DrawString(theLabel,BPoint(4,fontHeight.ascent+1));
}

float Button::dx() { return area.right; }

void Button::Draw(BRect) {
  knob(this,area,false);
  SetHighColor(textColor);
  drawLabel();
}

RotateChoice::RotateChoice(BRect rect,int mes,int d, uint32 resizingMode):
    BView(rect, 0, resizingMode, B_WILL_DRAW),
    theMessage(mes ? new BMessage(mes) : 0),
    dim(d),
    value(0) {
}

void RotateChoice::MouseDown(BPoint where) {
  SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS|B_NO_POINTER_HISTORY); 
  value=(value+1)%dim;
  Invalidate();
  if (theMessage) Window()->PostMessage(theMessage);
}

void RotateChoice::Draw(BRect r) {
  SetHighColor(cWhite);
  StrokeLine(BPoint(r.left,r.bottom),BPoint(r.right,r.bottom));
  StrokeLine(BPoint(r.right,r.top));
  SetHighColor(cDarkGrey);
  StrokeLine(BPoint(r.left,r.bottom),BPoint(r.left,r.top));
  StrokeLine(BPoint(r.right,r.top));
  draw(value);
}

TextControl::TextControl(BRect rect,int mes,uint32 sizingMode):
    BTextView(rect,"text",BRect(0,0,rect.Width(),0),sizingMode,B_WILL_DRAW),
    theMessage(new BMessage(mes)),
    xoff(0) {
  appFont->GetHeight(&fontHeight);
  SetFontAndColor(appFont,B_FONT_SIZE);
  ResizeTo(rect.Width(),fontHeight.ascent + fontHeight.descent + 4);
  area=Bounds();
  area.InsetBy(3,2);
  SetTextRect(area);
  SetWordWrap(false);
}

void TextControl::KeyDown(const char *bytes,int32 n) {
  if (bytes[0]==B_HOME) {
    xoff=0;
    SetTextRect(area);
  }
  else if (bytes[0]==B_END) {
    xoff=fmax(0,LineWidth()-area.Width()/2);
    SetTextRect(BRect(area.left-xoff,area.top,area.right,area.bottom));
  }
  else {
    int32 os,os2;
    GetSelection(&os,&os2);
    float x=PointAt(os).x;
    if (x > area.Width() - 5) {
      xoff += 10;
      SetTextRect(BRect(area.left-xoff,area.top,area.right,area.bottom));
    }
    else if (x <= area.left + 5) {
      xoff=fmax(xoff-10,0);
      SetTextRect(BRect(area.left-xoff,area.top,area.right,area.bottom));
    }
  }
  BTextView::KeyDown(bytes,n);
}

void TextControl::reset() {
  xoff=0;
  SetTextRect(area);
  MakeFocus(false);
}
  
void TextControl::InsertText(const char *text,int32 length,int32 os,const text_run_array *runs) {
  if (text[0]=='\n')
    Window()->PostMessage(theMessage);
  else
    BTextView::InsertText(text,length,os,runs);
}

void TextControl::FrameResized(float w,float h) {
  area=BRect(0,0,w,h);
  area.InsetBy(3,2);
  SetTextRect(BRect(area.left-xoff,area.top,area.right,area.bottom));
}

void TextControl::Draw(BRect rect) {
  sunk(this,rect);
  BTextView::Draw(rect);
}

UpdText::UpdText(BView *vp,BPoint p):
    theView(vp),top(p),textColor(cBlack) {
  buf[0]=0;
  appFont->GetHeight(&fontHeight);
}

UpdTextBg::UpdTextBg(BView *vp,BPoint p):
    UpdText(vp,p),bgColor(vp->ViewColor()) {
}

void UpdTextBg::write(const char *form,...) {
  BRect area(top.x,
             top.y-fontHeight.ascent,
             top.x+theView->StringWidth(buf),
             top.y+fontHeight.descent);
  if (form && form!=buf) {
    if (buf[0]) {
      theView->SetHighColor(theView->ViewColor());
      theView->FillRect(area);
    }
    va_list ap;
    va_start(ap,form);
    vsprintf(buf,form,ap);
    va_end(ap);
  }
  area.right=top.x+theView->StringWidth(buf);
  theView->SetHighColor(bgColor);
  theView->FillRect(area);
  theView->SetLowColor(bgColor);
  theView->SetHighColor(textColor);
  theView->DrawString(buf,top);
  theView->SetLowColor(theView->ViewColor());

  textColor=cBlack;   // reset
  bgColor=theView->ViewColor();
}

void UpdText::write(const char *form,...) {
  theView->SetLowColor(theView->ViewColor());
  if (form && form!=buf) {
    if (buf[0]) {
      theView->SetHighColor(theView->ViewColor());
      theView->DrawString(buf,top);
    }
    va_list ap;
    va_start(ap,form);
    vsprintf(buf,form,ap);
    va_end(ap);
  }
  theView->SetHighColor(textColor);
  theView->DrawString(buf,top);
}

struct Alert: BWindow {
  struct AlertView: BView {
    const char* text;
    Button *ok;
    AlertView(BRect rect,const char* txt):
        BView(rect,0,B_FOLLOW_NONE,B_WILL_DRAW),
        text(strcpy(new char[strlen(txt)+1],txt)),
        ok(new Button(BRect(10,20,0,0),"okay",'alrt')) {
      SetViewColor(cPink);
      SetLowColor(cPink);
      SetHighColor(cBlack);
      AddChild(ok);
    }
    void Draw(BRect rect) { DrawString(text,BPoint(5,12)); }
  };
  AlertView *view;
  int *nr;
  Alert(BRect rect,const char *txt,int *n):
      BWindow(rect,0,B_BORDERED_WINDOW,B_ASYNCHRONOUS_CONTROLS),
      view(new AlertView(Bounds(),txt)),
      nr(n) {
    AddChild(view);
  }
  void MessageReceived(BMessage* mes) {
    if (mes->what=='alrt') {
      delete const_cast<char*>(view->text);
      if (nr) --(*nr);
      Quit();
    }
    else
      BWindow::MessageReceived(mes);
  }
};

BWindow *call_alert(BRect rect,const char* text,int *nr=0) { 
  BWindow *a=new Alert(rect,text,nr);
  a->Show();
  return a;
}
