# GLFW Getting Started
### GLFW Hello World in Visual Studio Community 2022
* Create a console application with a subfolder for the project. Ensure the project builds and runs.
* Download the GLFW binary achive for Windows 64 and extract under the main project folder.
* The folder hierarchy should look this:
~~~
<ProjectsRoot>\GlfwGettingStarted\
	--> GlfwGettingStarted
	--> glfw-3.4.bin.WIN64
~~~
* Add the glfw include directory:
  * Project->GlfwGettingStarted Properties->Configuration Properties->C++->General
	* Additional Include  Directories, under the dropdown, Edit.
	* Add the relative include path. Note that the path that is added does not include the GLFW folder under the include folder, as that is specified in the #include <GLFW/glfw3.h> statement in code.
	* ..\\..\\Glfw\glfw-3.4.bin.WIN64\include\
* Add the glfw library directory:
	* Project->GlfwGettingStarted Properties->Configuration Properties->Linker->General
	* Additional Library Directories, under the dropdown, Edit.
	* Add the relative path:
	* ..\..\Glfw\glfw-3.4.bin.WIN64\lib-vc2022
* Add the glfw and openGl static libraries
  * Project->GlfwGettingStarted Properties->Configuration Properties->Linker->Input
	* Additional Dependencies, , under the dropdown, Edit.
	* Add these libraries:
		* glfw3_mt.lib
		* opengl32.lib
* Set the project to statically link the runtime library:
  * Project->GlfwGettingStarted Properties->Configuration Properties->C++->Code Generation
	* Runtime library->Multi-threaded Debug
* GLFW Preprocessor define:
	* Project->GlfwGettingStarted Properties->Configuration Properties->C++->Preporcessor Definitions
	* Add GLFW_INCLUDE_NONE 
* Copy replace the main code file with these contents:
~~~ cplusplus
#include <GLFW/glfw3.h>

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
~~~

### Common Errors:
* Linking to the dll vs static runtime library
~~~
Severity	Code	Description	Project	File	Line	Suppression State	Details
Warning	LNK4098	defaultlib 'MSVCRT' conflicts with use of other libs; use /NODEFAULTLIB:library	GlfwIntro	D:\Home\Scratch\Glfw\GlfwIntro\LINK	1		
~~~

* Not linking the opengl library:
~~~
Severity	Code	Description	Project	File	Line	Suppression State	Details
Error	LNK2019	unresolved external symbol __imp_glClear referenced in function main	GlfwIntro	D:\Home\Scratch\Glfw\GlfwIntro\GlfwIntro.obj	1	
~~~

* Opening VS2022 project in VS2019
~~~
1>C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Microsoft\VC\v160\Microsoft.CppBuild.targets(439,5): error MSB8020: The build tools for v143 (Platform Toolset = 'v143') cannot be found. To build using the v143 build tools, please install v143 build tools.  Alternatively, you may upgrade to the current Visual Studio tools by selecting the Project menu or right-click the solution, and then selecting "Retarget solution".
~~~
