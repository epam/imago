/****************************************************************************
 * Copyright (C) 2009-2010 GGA Software Services LLC
 * 
 * This file is part of Imago toolkit.
 * 
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 3 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 * 
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 ***************************************************************************/

#ifndef _chemical_structure_recognizer_h
#define _chemical_structure_recognizer_h

#include "image.h"
#include "character_recognizer.h"
#include "stl_fwd.h"

namespace imago
{
   class Molecule;
   class Segment;
   class CharacterRecognizer;
   
   class ChemicalStructureRecognizer
   {
   public:

      ChemicalStructureRecognizer();
      ChemicalStructureRecognizer( const char *fontfile );
      //ChemicalStructureRecognizer( const CharacterRecognizer &cr );

      void setImage( Image &img );
      void recognize( Molecule &mol, bool only_extract_characters = false ); 
      void image2mol( Image &img, Molecule &mol );
	  void extractCharacters (Image& img);
      const CharacterRecognizer &getCharacterRecognizer() { return  _cr; };

      ~ChemicalStructureRecognizer();

   private:
      CharacterRecognizer _cr;
      Image _origImage;
      
      ChemicalStructureRecognizer( const ChemicalStructureRecognizer &csr );
   };
}


#endif /* _chemical_structure_recognizer_h */
