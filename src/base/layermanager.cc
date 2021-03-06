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

#include "layermanager.hh"


PF::LayerManager::LayerManager( PF::Image* img ): image( img )
{
}


PF::LayerManager::~LayerManager()
{
  for(unsigned int i = 0; i < layers_pool.size(); i++) {
    if(layers_pool[i] == NULL) 
      continue;
    delete layers_pool[i];
  }
}


PF::Layer* PF::LayerManager::new_layer()
{
  uint32_t i;
  for(i = 0; i < layers_pool.size(); i++) {
    if(layers_pool[i] == NULL) {
      PF::Layer* l = new PF::Layer(i);
      l->set_image( image );
      layers_pool[i] = l;
      return l;
    }
  }
  PF::Layer* l = new PF::Layer(layers_pool.size());
  l->set_image( image );
  layers_pool.push_back(l);
  return l;
}


PF::Layer* PF::LayerManager::get_layer(int id)
{
  if(id < 0 || id >= (int)layers_pool.size()) return NULL;
  return ( layers_pool[id] );
}



bool PF::LayerManager::get_parent_layers(Layer* layer, 
					 std::list< std::pair<std::string,Layer*> >& plist,
					 std::string parent_name, std::list<Layer*>& list)
{  
  std::cout<<"Collecting parents of layer \""<<layer->get_name()<<"\"("<<layer->get_id()<<")"<<std::endl;
  std::list<PF::Layer*>::iterator li = list.begin();
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;
    std::string name;
    if( !parent_name.empty() ) name = parent_name + "/";
    name = name + l->get_name();
    std::cout<<"  checking layer \""<<l->get_name()<<"\"("<<l->get_id()<<")"<<std::endl;
    if( l->get_id() != layer->get_id() ) {
      plist.push_back( make_pair( name, l ) );
      std::cout<<"    added."<<std::endl;
    }

    if( get_parent_layers( layer, plist, name, l->sublayers ) )
      return true;

    if( get_parent_layers( layer, plist, name+"/IMap/", l->imap_layers ) )
      return true;

    if( get_parent_layers( layer, plist, name+"/OMap/", l->omap_layers ) )
      return true;

    if( l->get_id() == layer->get_id() ) 
      return true;
  }
  return false;
}


void PF::LayerManager::get_parent_layers(Layer* layer, 
					 std::list< std::pair<std::string,Layer*> >& plist)
{
  get_parent_layers( layer, plist, std::string(""), layers );
}




void PF::LayerManager::update_dirty( std::list<Layer*>& list, bool& dirty )
{  
  std::list<PF::Layer*>::iterator li = list.begin();
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;

    // if the current layer is dirty the dirty flag is set to true
    // this will also qualify as "dirty" all the subsequent layers in the list
    if( l->is_dirty() )
      dirty = true;

    // if the current layer is not qualified as "dirty", but one of the extra input layers is,
    // then we set the dirty flag to true as well
    if( !dirty ) {
      for( unsigned int i = 0; i < l->extra_inputs.size(); i++ ) {
	Layer* lextra = get_layer( l->extra_inputs[i] );
	if( lextra && lextra->is_dirty() ) {
	  dirty = true;
	  break;
	}
      }
    }

    // Now we walk through the intensity and opacity maps to see what needs to be re-built.
    // If either one or the other has to be re-built, then we mark this layer "dirty" as well
    bool imap_dirty = false;
    update_dirty( l->imap_layers, imap_dirty );

    bool omap_dirty = false;
    update_dirty( l->omap_layers, omap_dirty );

    if( imap_dirty || omap_dirty )
      dirty = true;

    // Finally we walk through the sub-layers; again, if re-building is needed 
    // we mark this layer "dirty" as well
    bool sub_dirty = false;
    update_dirty( l->sublayers, sub_dirty );

    if( sub_dirty )
      dirty = true;
    
    // Now we have finished walking through all the subchains and extra inputs,
    // and we can set the dirty flag for the current layer
    l->set_dirty( dirty );
  }
}




void PF::LayerManager::reset_dirty( std::list<Layer*>& list )
{  
  std::list<PF::Layer*>::iterator li = list.begin();
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;

    l->clear_dirty();

    reset_dirty( l->imap_layers );
    reset_dirty( l->omap_layers );
    reset_dirty( l->sublayers );
  }
}




bool PF::LayerManager::insert_layer( Layer* layer, int32_t lid )
{  
  if( lid < 0 ) {
    layers.push_back( layer );
    return true;
  }

  std::list<Layer*>::iterator it;
  for( it = layers.begin(); it != layers.end(); ++it )
    if( (*it)->get_id() == lid ) break;

  if( it == layers.end() ) return false;
  it++;
  layers.insert( it, layer );

  return true;
}




VipsImage* PF::LayerManager::rebuild_chain( PF::View* view, colorspace_t cs, 
					    int width, int height, 
					    std::list<PF::Layer*>& list, 
					    PF::Layer* previous_layer )
{ 
  ViewNode* previous_node = NULL;
  VipsImage* previous = NULL;

  VipsImage* out = NULL;
  std::list<PF::Layer*>::iterator li = list.begin();
  for(li = list.begin(); li != list.end(); ++li) {
    PF::Layer* l = *li;
    if( !l->is_visible() ) 
      continue;

    if( previous_layer ) {
      previous_node = view->get_node( previous_layer->get_id() );
      if( previous_node ) previous = previous_node->image;
    }

    OpParBase* par = l->get_processor()->get_par();
    if( par ) {
      std::cout<<"PF::LayerManager::rebuild_chain(): setting format for layer "<<l->get_name()
	       <<" to "<<view->get_format()<<std::endl;
      par->set_format( view->get_format() );
    }

    char* name = (char*)l->get_name().c_str();

    // first we build the chains for the intensity and opacity maps
    VipsImage* imap = NULL;
    std::cout<<"Layer \""<<l->get_name()<<"\""
	     <<"  imap_layers.size()="<<l->imap_layers.size()
	     <<"  omap_layers.size()="<<l->omap_layers.size()
	     <<std::endl;
    if( previous && !l->imap_layers.empty() ) {
      imap = rebuild_chain( view, PF_COLORSPACE_GRAYSCALE, 
			    previous->Xsize, previous->Ysize, 
			    l->imap_layers, NULL );
      if( !imap )
	return false;
      //std::list<PF::Layer*>::reverse_iterator map_i = l->imap_layers.rbegin();
      //if(map_i != l->imap_layers.rend()) 
      //imap = (*map_i)->get_processor()->get_par()->get_image();
    }
    VipsImage* omap = NULL;
    if( previous && !l->omap_layers.empty() ) {
      omap = rebuild_chain( view, PF_COLORSPACE_GRAYSCALE, 
			    previous->Xsize, previous->Ysize, 
			    l->omap_layers, NULL );
      if( !omap )
	return false;
      //std::list<PF::Layer*>::reverse_iterator map_i = l->omap_layers.rbegin();
      //if(map_i != l->omap_layers.rend()) 
      //omap = (*map_i)->get_processor()->get_par()->get_image();
    }

    /* At this point there are two possibilities:
       1. the layer has no sub-layers, in which case it is combined with the output
          of the previous layer plus any extra inputs it might have
       2. the layer has sub-layers, in which case we first build the sub-layers chain
          and then we combine it with the output of the previous layer
    */
    VipsImage* newimg = NULL;
    if( l->sublayers.empty() ) {
      std::vector<VipsImage*> in;
      if( par->needs_input() && !previous ) {
	// Here we have a problem: the operation we are trying to insert in the chain requires
	// a primary input image, but there is no previous image available... we give up
	return false;
      }
      if(previous)
	in.push_back(previous);

      // Now we loop on the vector of extra inputs, and we include the corresponding
      // images in the input vector
      for(uint32_t iextra = 0; iextra < l->extra_inputs.size(); iextra++) {
	PF::Layer* lextra = get_layer( l->extra_inputs[iextra] );
	// If the extra input layer is not found we have a problem, better to give up
	// with an error.
	if( !lextra ) return false;
	PF::ViewNode* extra_node = view->get_node( lextra->get_id() );
	if( !extra_node ) return false;
	VipsImage* extra_img = extra_node->image;
	//VipsImage* extra_img = lextra->get_processor()->get_par()->get_image();
	// Similarly, if the extra input layer has no valid image associated to it
	// we have a problem and we gve up
	if( !extra_img ) return false;
	in.push_back( extra_img );
      }

      // If the layer is at the beginning of the chain, we set hints about the desired
      // colorspace and pixel format using default values. 
      // The size and colorspace hints might be ignored by
      // the operation (for example in the case of an image from a file, where the
      // size and colorspace are dictated by the file content).
      // On the other hand, the pixel format hint should be strictly respected by all 
      // operators, as it defined the accuracy at which the final image is rendered.
      // If a previous image has been created already, hints are copied from it.
      if( li == list.begin() )
	par->set_image_hints( width, height, cs );
      /* Not needed, already called by vips_layer_build() from src/vips/layer.cc
      else if( previous )
	par->set_image_hints( previous );
      */

      if( par->get_config_ui() ) par->get_config_ui()->update_properties();
      std::cout<<"Building layer \""<<l->get_name()<<"\"..."<<std::endl;
      newimg = par->build( in, 0, imap, omap);
      std::cout<<"... done."<<std::endl;
      if( par->get_config_ui() ) par->get_config_ui()->update();
    } else {
      std::vector<VipsImage*> in;

      // we add the previous image to the list of inputs, even if it is NULL
      in.push_back( previous );

      // Then we build the chain for the sub-layers, passing the previous image as the
      // initial input
      VipsImage* isub = NULL;
      if( previous ) 
	isub = rebuild_chain( view, cs, 
			      previous->Xsize, previous->Ysize, 
			      l->sublayers, previous_layer );
      else
	isub = rebuild_chain( view, cs, 
			      previous->Xsize, previous->Ysize, 
			      l->sublayers, NULL );

      // we add the output of the sub-layers chain to the list of inputs, even if it is NULL
      in.push_back( isub );
      
      if( par->get_config_ui() ) par->get_config_ui()->update_properties();
      std::cout<<"Building layer \""<<l->get_name()<<"\"..."<<std::endl;
      newimg = par->build( in, 0, imap, omap);
      std::cout<<"... done."<<std::endl;
      if( par->get_config_ui() ) par->get_config_ui()->update();
    }

    if( newimg ) {
      if( previous_layer )
	view->set_node( newimg, l->get_id(), previous_layer->get_id() );
      else
	view->set_node( newimg, l->get_id(), -1 );
      //view.set_output( newimg );
      out = newimg;
      //previous = newimg;
      previous_layer = l;
    }
  }
  return out;
}


bool PF::LayerManager::rebuild_prepare()
{
  std::cout<<"PF::LayerManager::rebuild_prepare(): layers.size()="<<layers.size()<<std::endl;
  bool dirty = false;
  update_dirty( layers, dirty );

  if( !dirty ) {
    return false;
  }
  return true;
}


bool PF::LayerManager::rebuild(View* view, colorspace_t cs, int width, int height)
{
  VipsImage* output = rebuild_chain( view, cs, width, height, layers, NULL );
  view->set_output( output );
  view->update();
  return true;
}


bool PF::LayerManager::rebuild_finalize()
{
  reset_dirty( layers );
  return true;
}




bool PF::LayerManager::rebuild_all(View* view, colorspace_t cs, int width, int height)
{
  if( layers.empty() )
    return true;
  PF::Layer* l = *(layers.begin());
  l->set_dirty( true );
  
  if( !l->imap_layers.empty() ) {
    PF::Layer* ll = *(l->imap_layers.begin());
    ll->set_dirty( true );
  }
  if( !l->omap_layers.empty() ) {
    PF::Layer* ll = *(l->omap_layers.begin());
    ll->set_dirty( true );
  }
  if( !l->sublayers.empty() ) {
    PF::Layer* ll = *(l->sublayers.begin());
    ll->set_dirty( true );
  }

  bool dirty = false;
  update_dirty( layers, dirty );

  if( !dirty ) {
    return false;
  }

  VipsImage* output  = rebuild_chain( view, cs, width, height, layers, NULL );
  view->set_output( output );

  reset_dirty( layers );

  return true;
}



bool PF::LayerManager::save( std::ostream& ostr )
{
  int level = 1;
  std::list<PF::Layer*>::iterator li;
  for(li = layers.begin(); li != layers.end(); ++li) {
    PF::Layer* l = *li;
    if( !l->save( ostr, level ) ) return false;
  }
  return true;
}
