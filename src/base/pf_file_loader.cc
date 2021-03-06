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

#include <libgen.h>

#include <string>
#include <stack>
#include <vector>


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


#include "pf_file_loader.hh"

static PF::Image* image = NULL;
static std::deque<PF::Layer*> layers_stack;
static std::deque< std::list<PF::Layer*>* > containers_stack;
static PF::Layer* current_layer = NULL;
static std::list<PF::Layer*>* current_container = NULL;
static PF::OpParBase* current_op = NULL;

static std::vector<PF::Layer*> layer_id_mapper;


void append_layer( PF::Layer* layer, PF::Layer* previous, std::list<PF::Layer*>& list )
{
  if( !previous ) {
    list.push_back( layer );
    return;
  }
  
  std::list<PF::Layer*>::iterator i;
  for( i = list.begin(); i != list.end(); i++ ) {
    PF::Layer* l = (*i);
    if( !l ) continue;
    if( l->get_id() != previous->get_id() ) continue;
    break;
  }

  if( i == list.end() ) {
    list.push_back( layer );
  } else {
    i++;
    list.insert( i, layer );
  }
}

/* The handler functions. */

void start_element (GMarkupParseContext *context,
		    const gchar         *element_name,
		    const gchar        **attribute_names,
		    const gchar        **attribute_values,
		    gpointer             user_data,
		    GError             **error) {

  const gchar **name_cursor = attribute_names;
  const gchar **value_cursor = attribute_values;

  if( strcmp (element_name, "layer") == 0 ) {

    // Create a new layer
    PF::Layer* layer = image->get_layer_manager().new_layer();
    // The new layer is inserted in the current container, above the current one
    // If the current one is NULL, then the new layer is simply inserted on the top of the list
    // Useful when for example inserting the first layer or sublayer
    append_layer( layer, current_layer, *current_container );

    // Now we can update the current_layer pointer and we can collect parameters
    // The new layer is pushed on top of the layers stack
    current_layer = layer;
    layers_stack.push_back( layer );
    int old_id = -1;
    std::string name;
    int visible = -1;
    int normal = -1;
    std::vector<int> extra_inputs;

    while (*name_cursor) {
      if (strcmp (*name_cursor, "id") == 0) {
	old_id = atoi( *value_cursor );
      } else if (strcmp (*name_cursor, "name") == 0) {
	name = *value_cursor;
      } else if (strcmp (*name_cursor, "visible") == 0) {
	visible = atoi( *value_cursor );
      } else if (strcmp (*name_cursor, "normal") == 0) {
	normal = atoi( *value_cursor );
      } else if (strcmp (*name_cursor, "extra_inputs") == 0) {
	std::string idstr = *value_cursor;
	std::istringstream idstream( idstr );
	int ninput = 0;
	while ( !idstream.eof() ) {
	  int id;
	  idstream>>id;
	  if( !idstream ) 
	    break;
	  if( id < layer_id_mapper.size() &&
	      layer_id_mapper[id] )
	    layer->set_input( ninput, layer_id_mapper[id]->get_id() );
	  ninput++;
	}
      }
	
      name_cursor++;
      value_cursor++;
    }

    for(int i = layer_id_mapper.size(); i <= old_id; i++) 
      layer_id_mapper.push_back( NULL );
    layer_id_mapper[old_id] = layer;

    layer->set_name( name );
    layer->set_visible( visible );
    layer->set_normal( normal );

    std::cout<<"Layer \""<<layer->get_name()<<"\" extra inputs: ";
    for(unsigned int i = 0; i < layer->get_extra_inputs().size(); i++)
      std::cout<<layer->get_extra_inputs()[i]<<" ";
    std::cout<<std::endl;

  } else if( strcmp (element_name, "sublayers") == 0 ) {

    if( !current_layer ) return;

    std::string type;

    while (*name_cursor) {
      if (strcmp (*name_cursor, "type") == 0) {
	type = *value_cursor;
      }
	
      name_cursor++;
      value_cursor++;
    }

    if( type == "imap" ) {
      current_container = &(current_layer->get_imap_layers());
      containers_stack.push_back( current_container );
    } else if( type == "omap" ) {
      current_container = &(current_layer->get_omap_layers());
      containers_stack.push_back( current_container );
    } else if( type == "child" ) {
      current_container = &(current_layer->get_sublayers());
      containers_stack.push_back( current_container );
    }

    // At this point we set the current_layer pointer to NULL,
    // since we are starting to build a new layers list
    // The pointer will be restored from the layers stack
    // when we will exit the "sublayers" element
    current_layer = NULL;

  } else if( strcmp (element_name, "operation") == 0 ) {
    std::string op_type;
    while (*name_cursor) {
      if (strcmp (*name_cursor, "type") == 0) {
	op_type = *value_cursor;
      }
	
      name_cursor++;
      value_cursor++;
    }

    std::cout<<"PF::pf_file_loader(): creating operation of type "<<op_type<<std::endl;

    if( !current_layer ) return;

    PF::ProcessorBase* processor = PF::PhotoFlow::Instance().new_operation( op_type, current_layer );

    if( processor ) {
      std::cout<<"PF::pf_file_loader(): operation created."<<std::endl;
      current_op = processor->get_par();
    }

  } else if( strcmp (element_name, "property") == 0 ) {

    if( !current_op ) return;

    std::string pname;
    std::string pvalue;
    while (*name_cursor) {
      if (strcmp (*name_cursor, "name") == 0) {
	pname = *value_cursor;
      } else if (strcmp (*name_cursor, "value") == 0) {
	pvalue = *value_cursor;
      }
	
      name_cursor++;
      value_cursor++;
    }

    std::cout<<"PF::pf_file_loader(): setting property \""<<pname<<"\" to \""<<pvalue<<"\""<<std::endl;

    if( !pname.empty() && !pvalue.empty() ) {
      PF::PropertyBase* p = current_op->get_property( pname );
      if( p ) {
	p->set_str( pvalue );
      }
    }

  }
}

void text(GMarkupParseContext *context,
	  const gchar         *text,
	  gsize                text_len,
	  gpointer             user_data,
	  GError             **error)
{
  /* Note that "text" is not a regular C string: it is
   * not null-terminated. This is the reason for the
   * unusual %*s format below.
   */
  
}

void end_element (GMarkupParseContext *context,
		  const gchar         *element_name,
		  gpointer             user_data,
		  GError             **error)
{
  // If element is a layer, pop it from the stack
  if( strcmp (element_name, "layer") == 0 ) {

    // The current layer is removed from the stack, and the current_layer pointer
    // is set to the top element of the stack if present, or to NULL otherwise
    layers_stack.pop_back();
    current_layer = NULL;
    std::cout<<"\""<<element_name<<"\" end: layers_stack.size()="<<layers_stack.size()<<std::endl;
    if( !layers_stack.empty() ) 
      current_layer = layers_stack.back();

  } else if( strcmp (element_name, "sublayers") == 0 ) {

    containers_stack.pop_back();
    current_container = NULL;
    std::cout<<"\""<<element_name<<"\" end: containers_stack.size()="<<containers_stack.size()<<std::endl;
    if( !containers_stack.empty() ) 
      current_container = containers_stack.back();

    // We also restore the current_layer pointer to the top element of the stack (if present)
    current_layer = NULL;
    std::cout<<"\""<<element_name<<"\" end: layers_stack.size()="<<layers_stack.size()<<std::endl;
    if( !layers_stack.empty() ) 
      current_layer = layers_stack.back();

  } else if( strcmp (element_name, "operation") == 0 ) {

    if( current_op && current_op->get_config_ui() ) {
      current_op->get_config_ui()->init();
      current_op->get_config_ui()->update();
    }
    current_op = NULL;

  }
}

/* The list of what handler does what. */
static GMarkupParser parser = {
  start_element,
  end_element,
  text,
  NULL,
  NULL
};



void PF::load_pf_image( std::string filename, PF::Image* img ) {
  char *text;
  gsize length;
  GMarkupParseContext *context = g_markup_parse_context_new (
      &parser,
      (GMarkupParseFlags)0,
      NULL,
      NULL);

  image = img;
  current_layer = NULL;
  current_container = NULL;
  layers_stack.clear();
  containers_stack.clear();
  layer_id_mapper.clear();

  current_container = &(image->get_layer_manager().get_layers());
  containers_stack.push_back( current_container );

  /* seriously crummy error checking */

  if (g_file_get_contents (filename.c_str(), &text, &length, NULL) == FALSE) {
    printf("Couldn't load XML\n");
    exit(255);
  }

  char* fname = strdup(filename.c_str());
  char* dname = dirname( fname );
  if( dname ) chdir( dname ); 
  free( fname );

  if (g_markup_parse_context_parse (context, text, length, NULL) == FALSE) {
    printf("Parse failed\n");
    exit(255);
  }

  g_free(text);
  g_markup_parse_context_free (context);
}
