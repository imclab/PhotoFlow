/* 
 */

/*

    Copyright (C) 2014 Ferrero Andrea

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.


 */

/*

    These files are distributed with PhotoFlow - http://aferrero2707.github.io/PhotoFlow/

 */

#ifndef CURVE_EDITOR_HH
#define CURVE_EDITOR_HH

#include <gtkmm.h>

#include "../../base/splinecurve.hh"

#include "pfwidget.hh"

namespace PF {


  class CurveArea: public Gtk::DrawingArea
  {
    SplineCurve* curve;

    int selected_point;

    bool on_expose_event(GdkEventExpose* event);
  public:    
    CurveArea();

    void set_curve( SplineCurve* c ) { curve = c; }
    SplineCurve* get_curve() { return curve; }
    
    void set_selected_point( int ipt ) { selected_point = ipt; }
    
    int get_selected_point() { return selected_point; }
  };


  class CurveEditor: public Gtk::HBox, public PFWidget
  {
    Gtk::VBox box;
    Gtk::Label xlabel, ylabel;
    Gtk::Alignment xalign, yalign;
    Gtk::Adjustment xadjustment, yadjustment;
    Gtk::SpinButton xspinButton, yspinButton;

   CurveArea curveArea;

    int grabbed_point;

    bool button_pressed;

    bool inhibit_value_changed;

    bool handle_curve_events(GdkEvent* event);

  public:
    CurveEditor(OperationConfigUI* dialog, std::string pname );

    ~CurveEditor() {}

    void update_point();

    void get_value();
    void set_value();
  };


}

#endif
