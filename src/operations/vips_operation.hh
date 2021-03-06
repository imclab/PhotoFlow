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

#ifndef VIPS_OPERATION_HH
#define VIPS_OPERATION_HH

#include <string>

#include "../base/processor.hh"

#include "blender.hh"

namespace PF 
{

  class VipsOperationPar: public BlenderPar
  {
    std::string op_name;

  public:
    VipsOperationPar();

    void set_op(std::string name);

    void add_argument( GParamSpec *pspec, VipsArgumentClass *argument_class );

    /* Set processing hints:
       1. the intensity parameter makes no sense for an image, 
       creation of an intensity map is not allowed
       2. the operation can work without an input image;
       the blending will be set in this case to "passthrough" and the image
       data will be simply linked to the output
    */
    bool has_intensity() { return false; }
    bool needs_input() { return false; } /* to be fixed... */

    VipsImage* build(std::vector<VipsImage*>& in, int first, VipsImage* imap, VipsImage* omap);
  };

  

  template < OP_TEMPLATE_DEF > 
  class VipsOperationProc: public BlenderProc<OP_TEMPLATE_IMP>
  {
  };

}

#endif 


