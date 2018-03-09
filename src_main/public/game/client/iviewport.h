// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#if !defined(IVIEWPORT_H)
#define IVIEWPORT_H

#include "vgui/VGUI.h"
#include "viewport_panel_names.h"

class KeyValues;

the_interface IViewPortPanel {
 public:
  virtual ~IViewPortPanel(){};

  virtual const char *GetName(void) = 0;       // return identifer name
  virtual void SetData(KeyValues * data) = 0;  // set ViewPortPanel data
  virtual void Reset(void) = 0;   // clears internal state, deactivates it
  virtual void Update(void) = 0;  // updates all (size, position, content, etc)
  virtual bool NeedsUpdate(
      void) = 0;  // query panel if content needs to be updated
  virtual bool HasInputElements(
      void) = 0;  // true if panel contains elments which accepts input

  virtual void ShowPanel(bool state) = 0;  // activate VGUI Frame

  // VGUI functions:
  virtual vgui::VPANEL GetVPanel(void) = 0;  // returns VGUI panel handle
  virtual bool IsVisible() = 0;              // true if panel is visible
  virtual void SetParent(vgui::VPANEL parent) = 0;
};

the_interface IViewPort {
 public:
  virtual void UpdateAllPanels(void) = 0;
  virtual void ShowPanel(const char *pName, bool state) = 0;
  virtual void ShowPanel(IViewPortPanel * pPanel, bool state) = 0;
  virtual void ShowBackGround(bool bShow) = 0;
  virtual IViewPortPanel *FindPanelByName(const char *szPanelName) = 0;
  virtual IViewPortPanel *GetActivePanel(void) = 0;
  virtual void PostMessageToPanel(const char *pName, KeyValues *pKeyValues) = 0;
};

extern IViewPort *gViewPortInterface;

#endif  // IVIEWPORT_H
