#include "acrosAFE.h"

#include "rosAFE_c_types.h"

#include "genom3_dataFiles.hpp"

/* --- Function AddFlag ------------------------------------------------- */

/** Codel addFlag of function AddFlag.
 *
 * Returns genom_ok.
 */
genom_event
addFlag(char **lowerDep, char **upperDep, rosAFE_flagMap **flagMapSt,
        genom_context self)
{
	
  // Transformation to string
  std::string lowerDepS = *lowerDep;
  std::string upperDepS = *upperDep;
  
  // Creation of a flag
  flagStPtr flag ( new flagSt() );
  flag->upperDep = upperDepS;
  flag->lowerDep = lowerDepS;
  flag->waitFlag = true;
	  
  // Storing that flag into the flagMap
  (*flagMapSt)->allFlags.push_back( std::move( flag ) );
  
  return genom_ok;
}
