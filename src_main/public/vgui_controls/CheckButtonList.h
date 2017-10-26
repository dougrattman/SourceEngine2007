// Copyright © 1996-2017, Valve Corporation, All rights reserved.

#ifndef CHECKBUTTONLIST_H
#define CHECKBUTTONLIST_H

#include <vgui_controls/EditablePanel.h>
#include "tier1/UtlVector.h"

namespace vgui {
// Purpose: Contains a list of check boxes, displaying scrollbars if necessary
class CheckButtonList : public EditablePanel {
  DECLARE_CLASS_SIMPLE(CheckButtonList, EditablePanel);

 public:
  CheckButtonList(Panel *parent, const char *name);
  ~CheckButtonList();

  // adds a check button to the list
  int AddItem(const char *itemText, bool startsSelected, KeyValues *userData);

  // clears the list
  void RemoveAll();

  // number of items in list that are checked
  int GetCheckedItemCount();

  // item iteration
  bool IsItemIDValid(int itemID);
  int GetHighestItemID();
  int GetItemCount();

  // item info
  KeyValues *GetItemData(int itemID);
  bool IsItemChecked(int itemID);
  void SetItemCheckable(int itemID, bool state);

  /* MESSAGES SENT
          "CheckButtonChecked" - sent when one of the check buttons state has
     changed

  */

 protected:
  virtual void PerformLayout();
  virtual void ApplySchemeSettings(IScheme *pScheme);
  virtual void OnMouseWheeled(int delta);

 private:
  MESSAGE_FUNC_PARAMS(OnCheckButtonChecked, "CheckButtonChecked", pParams);
  MESSAGE_FUNC(OnScrollBarSliderMoved, "ScrollBarSliderMoved");

  struct CheckItem_t {
    vgui::CheckButton *checkButton;
    KeyValues *userData;
  };
  CUtlVector<CheckItem_t> m_CheckItems;
  vgui::ScrollBar *m_pScrollBar;
};

}  // namespace vgui

#endif  // CHECKBUTTONLIST_H
