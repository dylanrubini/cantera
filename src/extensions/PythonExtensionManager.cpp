//! @file PythonExtensionManager.cpp

// This file is part of Cantera. See License.txt in the top-level directory or
// at https://cantera.org/license.txt for license and copyright information.

#include "cantera/extensions/PythonExtensionManager.h"

#include "cantera/kinetics/ReactionRateFactory.h"
#include "cantera/kinetics/ReactionRateDelegator.h"
#include "pythonExtensions.h" // generated by Cython

#include <boost/algorithm/string.hpp>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ba = boost::algorithm;
using namespace std;

namespace {

class PythonHandle : public Cantera::ExternalHandle
{
public:
    explicit PythonHandle(PyObject* obj) : m_obj(obj) {}

    ~PythonHandle() {
        Py_XDECREF(m_obj);
    }

private:
    PyObject* m_obj;
};


std::string getPythonExceptionInfo()
{
    if (!PyErr_Occurred()) {
        return "no Python exception raised";
    }

    PyObject* ex_type;
    PyObject* ex_value;
    PyObject* traceback;
    PyErr_Fetch(&ex_type, &ex_value, &traceback);
    PyErr_NormalizeException(&ex_type, &ex_value, &traceback);
    if (traceback == nullptr) {
        traceback = Py_None;
    }
    char* c_exstr = ct_getExceptionString(ex_type, ex_value, traceback);
    string message;
    if (c_exstr != nullptr) {
        message = c_exstr;
        free(c_exstr);
    } else {
        message = "Couldn't get exception message";
    }
    Py_XDECREF(ex_type);
    Py_XDECREF(ex_value);
    Py_XDECREF(traceback);
    return message;
}

} // end anonymous namespace

namespace Cantera
{

bool PythonExtensionManager::s_imported = false;

PythonExtensionManager::PythonExtensionManager()
{
    if (!Py_IsInitialized()) {
        // Update the path to include the virtual environment, if one is active
        const char* venv_path = getenv("VIRTUAL_ENV");
        if (venv_path != nullptr) {
            #ifdef _WIN32
                string suffix = "\\Scripts\\python.exe";
            #else
                string suffix = "/bin/python";
            #endif
            string path(venv_path);
            path += suffix;
            wstring wpath = wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(path);
            Py_SetProgramName(wpath.c_str());
        } else {
            #if defined(CT_PYTHONHOME) && defined(_WIN32)
                const char* old_pythonhome = getenv("PYTHONHOME");
                if (old_pythonhome == nullptr || old_pythonhome[0] == '\0') {
                    string pythonhome = "PYTHONHOME=";
                    pythonhome += CT_PYTHONHOME;
                    _putenv(pythonhome.c_str());
                }
            #endif
        }
        Py_Initialize();
    }

    if (s_imported) {
        return;
    }

    // PEP 489 Multi-phase initialization

    // The 'pythonExtensions' Cython module defines some functions that are used
    // to instantiate ExtensibleSomething objects.
    PyModuleDef* modDef = (PyModuleDef*) PyInit_pythonExtensions();
    if (!modDef->m_slots || !PyModuleDef_Init(modDef)) {
        if (PyErr_Occurred()) {
            PyErr_PrintEx(0);
        }
        throw CanteraError("PythonExtensionManager::PythonExtensionManager",
                           "Failed to import 'pythonExtensions' module");
    }

    // Following example creation of minimal ModuleSpec from Python's import.c
    PyObject *attrs = Py_BuildValue("{ss}", "name", "pythonExtensions");
    if (attrs == nullptr) {
        if (PyErr_Occurred()) {
            PyErr_PrintEx(0);
        }
        throw CanteraError("PythonExtensionManager::PythonExtensionManager",
                           "Py_BuildValue failed");
    }
    PyObject *spec = _PyNamespace_New(attrs);
    Py_DECREF(attrs);
    if (spec == nullptr) {
        if (PyErr_Occurred()) {
            PyErr_PrintEx(0);
        }
        throw CanteraError("PythonExtensionManager::PythonExtensionManager",
                           "_PyNamespace_New failed");
    }
    PyObject* pyModule = PyModule_FromDefAndSpec(modDef, spec);
    if (pyModule == nullptr) {
        if (PyErr_Occurred()) {
            PyErr_PrintEx(0);
        }
        CanteraError("PythonExtensionManager::PythonExtensionManager",
                     "PyModule_FromDefAndSpec failed");
    }
    int code = PyModule_ExecDef(pyModule, modDef);
    if (code) {
        if (PyErr_Occurred()) {
            PyErr_PrintEx(0);
        }
        CanteraError("PythonExtensionManager::PythonExtensionManager",
                     "PyModule_ExecDef failed");
    }
    Py_DECREF(spec);
    Py_DECREF(pyModule);
    s_imported = true;
}

void PythonExtensionManager::registerRateBuilders(const string& extensionName)
{
    // Each rate builder class is decorated with @extension, which calls the
    // registerPythonRateBuilder method to register that class. So all we have
    // to do here is load the module.
    PyObject* module_name = PyUnicode_FromString(extensionName.c_str());
    PyObject* py_module = PyImport_Import(module_name);
    Py_DECREF(module_name);
    if (py_module == nullptr) {
        throw CanteraError("PythonExtensionManager::registerRateBuilders",
                           "Problem loading module:\n{}", getPythonExceptionInfo());
    }
    ct_registerReactionDelegators();
}

void PythonExtensionManager::registerPythonRateBuilder(
    const std::string& moduleName, const std::string& className,
    const std::string& rateName)
{
    // Make sure the helper module has been loaded
    PythonExtensionManager mgr;

    // Create a function that constructs and links a C++ ReactionRateDelegator
    // object and a Python ExtensibleRate object of a particular type, and register
    // this as the builder for reactions of this type
    auto builder = [moduleName, className](const AnyMap& params, const UnitStack& units) {
        auto delegator = make_unique<ReactionRateDelegator>();
        PyObject* extRate = ct_newPythonExtensibleRate(delegator.get(),
                moduleName, className);
        if (extRate == nullptr) {
            throw CanteraError("PythonExtensionManager::registerRateBuilders",
                                "Problem in ct_newPythonExtensibleRate:\n{}",
                                getPythonExceptionInfo());
        }
        //! Call setParameters after the delegated functions have been connected
        delegator->setParameters(params, units);

        // The delegator is responsible for eventually deleting the Python object
        delegator->holdExternalHandle(make_shared<PythonHandle>(extRate));
        return delegator.release();
    };
    ReactionRateFactory::factory()->reg(rateName, builder);
}

};
