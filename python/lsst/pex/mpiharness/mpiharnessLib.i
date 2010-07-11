/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */
 
%define mpiharness_DOCSTRING
"
Access to the C++ harness classes from the lsst.pex.mpiharness module
"
%enddef

%feature("autodoc", "1");
%module(package="lsst.pex.mpiharness", docstring=mpiharness_DOCSTRING,  "directors=1") mpiharnessLib


%{
#include "lsst/daf/base/Citizen.h"
#include "lsst/pex/exceptions.h"
#include "lsst/pex/policy/Policy.h"
#include "lsst/pex/policy/Dictionary.h"
#include "lsst/daf/base/PropertySet.h"
#include "lsst/daf/persistence/PropertySetFormatter.h"
#include "lsst/pex/logging/Log.h"
#include "lsst/pex/logging/LogRecord.h"
#include "lsst/pex/logging/Debug.h"
#include "lsst/pex/mpiharness/Pipeline.h"
#include "lsst/pex/mpiharness/Slice.h"
#include "lsst/pex/harness/TracingLog.h"
%}

%inline %{
namespace lsst { namespace pex { namespace mpiharness { } } }
namespace lsst { namespace pex { namespace harness { } } }
namespace lsst { namespace daf { namespace base { } } }
namespace lsst { namespace daf { namespace persistence { } } }
namespace lsst { namespace pex { namespace policy { } } }
namespace lsst { namespace pex { namespace exceptions { } } }
namespace boost { namespace filesystem {} }

using namespace lsst;
using namespace lsst::pex::mpiharness;
using namespace lsst::pex::harness;
using namespace lsst::daf::base;
using namespace lsst::daf::persistence;
using namespace lsst::pex::policy;
using namespace lsst::pex::exceptions;
%}

%init %{
%}

%pythoncode %{
import lsst.daf.base
import lsst.daf.persistence
import lsst.pex.policy
import lsst.pex.harness
import lsst.pex.mpiharness
%}


%include "lsst/p_lsstSwig.i"
%lsst_exceptions()

%include "std_string.i"
%include "std_set.i"
%include "lsst/utils/Utils.h"

%import "lsst/daf/base/baseLib.i"
%import "lsst/pex/logging/loggingLib.i"
%import "lsst/pex/policy/policyLib.i"
%import "lsst/pex/harness/harnessLib.i"


%import "lsst/daf/base/Citizen.h"
%import "lsst/daf/base/PropertySet.h"
%import "lsst/daf/persistence/PropertySetFormatter.h"
%import "lsst/pex/exceptions.h"
%import "lsst/pex/logging/Debug.h"
%import "lsst/pex/logging/Log.h"
%import "lsst/pex/logging/LogRecord.h"
%import "lsst/pex/policy/Policy.h"
%import "lsst/pex/harness/TracingLog.h"

%include "lsst/pex/mpiharness/Pipeline.h"
%include "lsst/pex/mpiharness/Slice.h"

