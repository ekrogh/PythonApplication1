// At the top of your file, with other includes
#include "rnnoise.h"
#include <iostream>
#include <vector>
#include <string>
#include <Python.h>
#include <windows.h> // Add this include at the top of your file


// Return the directory containing the current executable (per-OS).
static std::string app_dir()
{
#if defined(_WIN32)
	char path[MAX_PATH] = { 0 };
	DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
	if (len == 0 || len == MAX_PATH) return std::string();
	std::string exePath(path, len);
	size_t pos = exePath.find_last_of("/\\");
	if (pos == std::string::npos) return std::string();
	return exePath.substr(0, pos);
#elif defined(__APPLE__)
	char path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) != 0) return std::string();
	std::string exePath(path);
	size_t pos = exePath.find_last_of("/\\");
	if (pos == std::string::npos) return std::string();
	return exePath.substr(0, pos);
#else // Linux, etc.
	char path[1024] = { 0 };
	ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (len <= 0) return std::string();
	std::string exePath(path, len);
	size_t pos = exePath.find_last_of("/\\");
	if (pos == std::string::npos) return std::string();
	return exePath.substr(0, pos);
#endif
}

// Helper to get the absolute path to the solution directory based on this source file path.
static std::string get_solution_dir()
{
	std::string file = __FILE__;
	// Trim to the directory containing this .cpp (ConsoleApplication1)
	size_t pos = file.find_last_of("/\\");
	if (pos == std::string::npos) return std::string();
	std::string projDir = file.substr(0, pos);
	// Trim once more to get the parent directory (solution root in this workspace layout)
	pos = projDir.find_last_of("/\\");
	if (pos == std::string::npos) return projDir;
	return projDir.substr(0, pos);
}

static bool init_embedded_python(const std::vector<std::string>& module_search_paths)
{
	PyStatus status;
	PyConfig config;
	PyConfig_InitIsolatedConfig(&config); // no user site, no env
	// Optional toggles
	config.parse_argv = 0;
	config.site_import = 0; // set 1 if you want site
	config.install_signal_handlers = 0;

	// Set home (folder where libpython + stdlib live). Adjust per platform below.
	const std::string base = app_dir();

#if defined(_WIN32)
	// Example layout:
	// MyApp.exe
	// python/ (contains python313.dll, python3.dll, python313.zip, python313._pth)
	// app/    (your .py code)
	const std::string py_home = base + "/python";
#elif defined(__APPLE__)
	// macOS app bundle:
	// MyApp.app/Contents/MacOS/MyApp (base)
	// MyApp.app/Contents/Frameworks/libpython3.13.dylib
	// MyApp.app/Contents/Resources/python (stdlib zip or folder)
	// MyApp.app/Contents/Resources/app     (your .py)
	const std::string py_home = base + "/../Resources/python";
#else
	// Linux / Android / others: keep python next to the binary or in resources unpacked at first run
	const std::string py_home = base + "/python";
#endif

	status = PyConfig_SetString(&config, &config.home, Py_DecodeLocale(py_home.c_str(), nullptr));
	if (PyStatus_Exception(status)) return false;

	// Provide module_search_paths explicitly (stdlib zip and your app code).
	// On Windows with embeddable, python313._pth can also control this; here we do it programmatically.
	status = PyConfig_SetBytesString(&config, &config.program_name, "MyApp");
	if (PyStatus_Exception(status)) return false;

	// Clear and set search paths
	// status = PyWideStringList_Clear(&config.module_search_paths);
	config.module_search_paths.length = 0;
	config.module_search_paths.items = nullptr;

	for (const auto& p : module_search_paths)
	{
		std::wstring wp(p.begin(), p.end());
		status = PyWideStringList_Append(&config.module_search_paths, wp.c_str());
		if (PyStatus_Exception(status)) return false;
	}

	status = Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);
	if (PyStatus_Exception(status)) return false;

	return true;
}

// A simple function demonstrating rnnoise usage
void run_rnnoise_example()
{
    // Create a DenoiseState object. This holds all the state for the neural network.
    DenoiseState* st = rnnoise_create(NULL);

    // The audio frame must be exactly 480 samples of 16-bit PCM audio.
    const int FRAME_SIZE = 480;
    short pcm_in[FRAME_SIZE] = { 0 }; // Your noisy audio data goes here
    short pcm_out[FRAME_SIZE] = { 0 };

    // Convert short PCM to float for rnnoise
    std::vector<float> float_in(FRAME_SIZE);
    std::vector<float> float_out(FRAME_SIZE);

    for (int i = 0; i < FRAME_SIZE; ++i)
        float_in[i] = static_cast<float>(pcm_in[i]);

    // This is the core processing function. It applies the noise suppression.
    // It returns a VAD (Voice Activity Detection) probability.
    float vad_prob = rnnoise_process_frame(st, float_out.data(), float_in.data());

    // Convert float output back to short PCM
    for (int i = 0; i < FRAME_SIZE; ++i)
        pcm_out[i] = static_cast<short>(float_out[i]);

    std::cout << "Processed a frame of audio. VAD probability: " << vad_prob << std::endl;

    // When you're done, you must destroy the state to free memory.
    rnnoise_destroy(st);
}

int main()
{
    // You can call the example function from main()
    run_rnnoise_example();

	std::cout << "Calling Python to find the sum of 2 and 2.\n";

	// Determine paths for Python modules
	const std::string solutionDir = get_solution_dir();
	const std::string pyFolder = solutionDir + "/Sample";

	// Initialize embedded Python with our search paths; fallback to default init.
	{
		std::vector<std::string> searchPaths;
		searchPaths.push_back(pyFolder);
		if (!init_embedded_python(searchPaths))
		{
			Py_Initialize();
		}
	}

	// Ensure our Python module folder is on sys.path
	{
		PyObject* sysPath = PySys_GetObject("path"); // borrowed
		if (sysPath)
		{
			PyObject* pPath = PyUnicode_FromString(pyFolder.c_str());
			if (pPath)
			{
				PyList_Insert(sysPath, 0, pPath); // add at front
				Py_DECREF(pPath);
			}
			else
			{
				PyErr_Print();
				std::cerr << "Failed to create Unicode path for sys.path.\n";
				Py_Finalize();
				return 1;
			}
		}
		else
		{
			PyErr_Print();
			std::cerr << "Failed to get sys.path.\n";
			Py_Finalize();
			return 1;
		}
	}

	// Import the Python module named "Sample".
	PyObject* pName = PyUnicode_FromString("Sample");
	if (pName == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to create Python string for module name.\n";
		Py_Finalize();
		return 1;
	}

	PyObject* pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	if (pModule == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to load Python module.\n";
		Py_Finalize();
		return 1;
	}

	// Get the dictionary of the module.
	PyObject* pDict = PyModule_GetDict(pModule); // borrowed
	if (pDict == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to get module dictionary.\n";
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}

	// Get the function named "add" from the module.
	PyObject* pFunc = PyDict_GetItemString(pDict, "add"); // borrowed
	if (pFunc == nullptr || !PyCallable_Check(pFunc))
	{
		PyErr_Print();
		std::cerr << "Failed to get callable function 'add'.\n";
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}

	// Prepare the arguments for the function call.
	PyObject* pArgs = PyTuple_New(2);
	if (pArgs == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to create tuple for arguments.\n";
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}

	PyObject* pValue = PyLong_FromLong(2);
	if (pValue == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to create Python integer for argument.\n";
		Py_DECREF(pArgs);
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}
	PyTuple_SetItem(pArgs, 0, pValue); // tuple steals reference

	pValue = PyLong_FromLong(2);
	if (pValue == nullptr)
	{
		PyErr_Print();
		std::cerr << "Failed to create Python integer for argument.\n";
		Py_DECREF(pArgs);
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}
	PyTuple_SetItem(pArgs, 1, pValue); // tuple steals reference

	// Call the function with the arguments.
	PyObject* pResult = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pArgs);
	if (pResult == nullptr)
	{
		PyErr_Print();
		std::cerr << "Function call failed.\n";
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}

	long long result = PyLong_AsLongLong(pResult);
	if (PyErr_Occurred())
	{
		PyErr_Print();
		std::cerr << "Failed to convert result to integer.\n";
		Py_DECREF(pResult);
		Py_DECREF(pModule);
		Py_Finalize();
		return 1;
	}
	std::cout << "add(2, 2) returned: " << result << "\n";

	Py_DECREF(pResult);
	Py_DECREF(pModule);

	Py_Finalize();

	return 0;
}