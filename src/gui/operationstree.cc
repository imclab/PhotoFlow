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

#include <iostream>
#include <vips/vips.h>

#include "../base/photoflow.hh"

#include "operationstree.hh"
#include "layerwidget.hh"

#include "../operations/vips_operation.hh"
#include "../operations/image_reader.hh"
#include "../operations/brightness_contrast.hh"
#include "../operations/invert.hh"
#include "../operations/gradient.hh"
#include "../operations/convert2lab.hh"
#include "../operations/clone.hh"
#include "../operations/curves.hh"

#include "../gui/operations/brightness_contrast_config.hh"
#include "../gui/operations/imageread_config.hh"
#include "../gui/operations/vips_operation_config.hh"
#include "../gui/operations/clone_config.hh"
#include "../gui/operations/curves_config.hh"

PF::OperationsTree::OperationsTree( )
{
  treeModel = Gtk::TreeStore::create(columns);
  set_model(treeModel);
  append_column("Name", get_columns().col_name);
}



PF::OperationsTree::~OperationsTree()
{
}


static std::map< std::string, std::list<std::string> > vips_operations;
static std::string vips_category = "";

static void* collect_class( GType type )
{
  const int depth = vips_type_depth( type );
  VipsObjectClass *klass = VIPS_OBJECT_CLASS( g_type_class_ref( type ) );
  bool abstract = G_TYPE_IS_ABSTRACT( type );

  std::cout<<"depth: "<<depth<<"  nickname: "<<klass->nickname;
  if( !abstract ) 
    std::cout<<"  concrete"<<std::endl;
  else
    std::cout<<"  abstract"<<std::endl;

  if( depth == 2 && abstract ) {
    std::map< std::string, std::list<std::string> >::iterator i = 
      vips_operations.find( klass->nickname );
    if(i == vips_operations.end() ) {
      vips_operations.insert( make_pair( std::string(klass->nickname), 
					 std::list<std::string>() ) );
      i = vips_operations.find( klass->nickname );
      if(i == vips_operations.end() ) 
	return( NULL );
    }
    vips_category = klass->nickname;
  } else if( depth >= 2 && !abstract ) {
    std::map< std::string, std::list<std::string> >::iterator i = 
      vips_operations.find( vips_category );
    if(i == vips_operations.end() ) 
      return( NULL );

    /* We need an instance to be able to call get_flags().
     */
    VipsOperation* operation = VIPS_OPERATION( g_object_new( type, NULL ) );
    VipsOperationFlags flags = vips_operation_get_flags( operation );

    int nout = 0;
    VIPS_ARGUMENT_FOR_ALL( operation, 
			   pspec, argument_class, argument_instance ) {
      
      g_assert( argument_instance );
      
      if( (argument_class->flags & VIPS_ARGUMENT_OUTPUT) ) {
	GType otype = G_PARAM_SPEC_VALUE_TYPE( pspec );
	const gchar* arg_name = g_param_spec_get_name( pspec );
	
	if( g_type_is_a( otype, VIPS_TYPE_IMAGE ) ) nout += 1;	  
      }
    } 
    VIPS_ARGUMENT_FOR_ALL_END;
    
    std::cout<<"klass->nickname -> # of output images: "<<nout<<std::endl;

    g_object_unref( operation ); 

    if( (nout==1) && !(flags & VIPS_OPERATION_DEPRECATED) ) 
      i->second.push_back( klass->nickname );
  }

  return( NULL );
  
}

void PF::OperationsTree::update_model()
{
  std::cout<<"PF::OperationsTree::update_model(): clearing tree"<<std::endl;
  treeModel->clear();
  vips_operations.clear();

  vips_type_map_all( g_type_from_name( "VipsOperation" ), 
		     (VipsTypeMapFn) collect_class, NULL );

  Gtk::TreeModel::Row row;
  Gtk::TreeModel::Row group;

  row = *(treeModel->append());
  row[columns.col_name] = "Open image";
  row[columns.col_nickname] = "imageread";

  row = *(treeModel->append());
  row[columns.col_name] = "Clone layer";
  row[columns.col_nickname] = "clone";

  row = *(treeModel->append());
  row[columns.col_name] = "Invert";
  row[columns.col_nickname] = "invert";

  row = *(treeModel->append());
  row[columns.col_name] = "Brightness/Contrast";
  row[columns.col_nickname] = "brightness_contrast";

  row = *(treeModel->append());
  row[columns.col_name] = "Curves";
  row[columns.col_nickname] = "curves";

  row = *(treeModel->append());
  row[columns.col_name] = "Lab conversion";
  row[columns.col_nickname] = "convert2lab";

  row = *(treeModel->append());
  row[columns.col_name] = "-------------------";

  row_vips =  *(treeModel->append());
  row_vips[columns.col_name] = "VIPS";

  std::map< std::string, std::list<std::string> >::iterator i;
  for( i = vips_operations.begin(); i != vips_operations.end(); i++ ) {
    group = *(treeModel->append(row_vips.children()));
    group[columns.col_name] = i->first.c_str();
    std::list<std::string>::iterator j;
    for( j = i->second.begin(); j != i->second.end(); j++ ) {
      row = *(treeModel->append(group.children()));
      row[columns.col_name] = (*j).c_str();
      row[columns.col_nickname] = (*j).c_str();
    }
  }



  //expand_all();
}






PF::OperationsTreeDialog::OperationsTreeDialog( Image* img, LayerWidget* lw ):
  Gtk::Dialog("New Layer",true),
  image( img ),
  layer_widget( lw )
{
  set_default_size(300,600);

  add_button("OK",1);
  add_button("Cancel",0);

  signal_response().connect(sigc::mem_fun(*this,
					  &OperationsTreeDialog::on_button_clicked) );

  op_tree_box.add( op_tree );
  get_vbox()->pack_start( op_tree_box );

  op_tree.signal_row_activated().connect( sigc::mem_fun(*this, &PF::OperationsTreeDialog::on_row_activated) );

  show_all_children();
}


PF::OperationsTreeDialog::~OperationsTreeDialog()
{
}


void PF::OperationsTreeDialog::open()
{
  op_tree.update_model();
  show_all();
}


void PF::OperationsTreeDialog::on_button_clicked(int id)
{
  switch(id) {
  case 0:
    hide_all();
    break;
  case 1:
    add_layer();
    hide_all();
    break;
  }
}


void PF::OperationsTreeDialog::on_row_activated( const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column )
{
  add_layer();
  hide_all();
  /*
  Gtk::TreeModel::iterator iter = op_tree.get_model()->get_iter( path );
  if (iter) {
    PF::OperationsTreeColumns& columns = op_tree.get_columns();
    Glib::ustring nickname = (*iter)[columns.col_nickname];
    std::cout<<"Adding layer of type \""<<(*iter)[columns.col_name]<<"\""
	     <<" ("<<(*iter)[columns.col_nickname]<<")"<<std::endl;
    //std::cout<<"Activated row "<<l->get_name()<<std::endl;

    //PF::OperationConfigUI* dialog = l->get_processor()->get_par()->get_config_ui();
    //if(dialog) dialog->open();
  }
  */
}



void PF::OperationsTreeDialog::add_layer()
{
  Glib::RefPtr<Gtk::TreeSelection> refTreeSelection =
    op_tree.get_selection();
  Gtk::TreeStore::iterator iter = refTreeSelection->get_selected();
  if( !iter ) return;

  PF::OperationsTreeColumns& columns = op_tree.get_columns();
  std::cout<<"Adding layer of type \""<<(*iter)[columns.col_name]<<"\""
	   <<" ("<<(*iter)[columns.col_nickname]<<")"<<std::endl;
  
  if( !image ) return;

  PF::LayerManager& layer_manager = image->get_layer_manager();
  PF::Layer* layer = layer_manager.new_layer();
  if( !layer ) return;
  layer->set_name( "New Layer" );

  Glib::ustring op_type = (*iter)[columns.col_nickname];
  PF::ProcessorBase* processor = 
    PF::PhotoFlow::Instance().new_operation( op_type.c_str(), layer );
  if( !processor || !processor->get_par() ) return;
  PF::OperationConfigUI* dialog = dynamic_cast<PF::OperationConfigUI*>( processor->get_par()->get_config_ui() );

  /*
  if( (*iter)[columns.col_nickname] == "imageread" ) { 

    processor = new PF::Processor<PF::ImageReaderPar,PF::ImageReader>();
    layer->set_processor( processor );
    dialog = new PF::ImageReadConfigDialog( layer );

  } else if( (*iter)[columns.col_nickname] == "clone" ) {

    processor = new PF::Processor<PF::ClonePar,PF::CloneProc>();
    layer->set_processor( processor );
    dialog = new PF::CloneConfigDialog( layer );

  } else if( (*iter)[columns.col_nickname] == "invert" ) {

    processor = new PF::Processor<PF::InvertPar,PF::Invert>();
    layer->set_processor( processor );
    dialog = new PF::OperationConfigDialog( layer, "Invert Image" );

  } else if( (*iter)[columns.col_nickname] == "brightness_contrast" ) {

    processor = new PF::Processor<PF::BrightnessContrastPar,PF::BrightnessContrast>();
    layer->set_processor( processor );
    dialog = new PF::BrightnessContrastConfigDialog( layer );

  } else if( (*iter)[columns.col_nickname] == "curves" ) {

    processor = new PF::Processor<PF::CurvesPar,PF::Curves>();
    layer->set_processor( processor );
    dialog = new PF::CurvesConfigDialog( layer );

  } else if( (*iter)[columns.col_nickname] == "convert2lab" ) {

    processor = new PF::Processor<PF::Convert2LabPar,PF::Convert2LabProc>();
    layer->set_processor( processor );
    //dialog = new PF::BrightnessContrastConfigDialog( layer );

  } else { // it must be a VIPS operation...

    PF::Processor<PF::VipsOperationPar,PF::VipsOperationProc>* vips_op = 
      new PF::Processor<PF::VipsOperationPar,PF::VipsOperationProc>();
    Glib::ustring str = (*iter)[columns.col_nickname];
    vips_op->get_par()->set_op( str.c_str() );
    processor = vips_op;
    layer->set_processor( processor );

    PF::VipsOperationConfigDialog* vips_config = 
      new PF::VipsOperationConfigDialog( layer );
    vips_config->set_op( str.c_str() );
    dialog = vips_config;
  }
  */

  if( processor ) {
    layer_widget->add_layer( layer );
    //layer_manager.get_layers().push_back( layer );
    //layer_manager.modified();
    if( dialog ) {
      //processor->get_par()->set_config_ui( dialog );
      dialog->update();
      dialog->open();
    }
  }
}
