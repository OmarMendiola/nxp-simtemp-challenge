# AI Conversation Notes: Simtemp Driver & Application

This file logs the user prompts provided during the design, development, and testing of the `nxp_simtemp` kernel driver, user-space application, and related scripts.

**Note:** This list may be incomplete. Due to conversation history limitations, older prompts might be missing.

## Driver Creation (Design & Documentation)

* `Te paso el código, el make file esta en simtemp/kernel` (Along with file uploads)
* `que tendría que hacer para cumplir con este requerimiento: - **Build:** out‑of‑tree module via \`KDIR=/lib/modules/\$(uname -r)/build\`. ... (Makefile KDIR info) ...\`
* `que significa que no hizo el BFT generation : ... (log de BTF skipping) ...`
* `Vamos a crear el archivo README.md en ingles, que incluya lo siguiente: exact build/run steps. **It MUST include the links to the video and git repo**`
* `Markdown` (Requesting README.md conversion to HTML)
* `Dame el readme en .md puro para poder copiar y pegar.`
* `generalo en el chat` (For README.md)
* `create the DESIGN.md with the following caracteristics: ... (Problem‑Solving Write‑Up requirements) ...`
* `I Will add some comments in the design, adjust the design acording this comments: ... (design justification for slow sampling rate) ...`
* `Give me the file in the chat with markdown format` (For DESIGN.md)

## Application Creation (User Logic & Debugging)

* `falta aplicar la zona hororara, son lsa 3:42 pm ... (logs con timestamps UTC) ...`
* `Haz el ajuste en codigo` (For timezone)
* `para le programa de Python para todas las funcionalidades se necesitarn permisos de root, incluso para solo leer el /dev/simptemp. Agrega también lo que se necesita para correr el script de python`
* `no debe tener __main__ se debe correr a través de main.py`
* `dame el comando para iniciar el debug desde la consola para después coenctarme con visual code`
* `solo que tengo el debugpy en omarm\WSL2-Linux-Kernel\simtemp\.venv`
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp$ pip install debugpy ... (Error: No module named debugpy con sudo) ...`

## Script & Test Creation (Build, Test, Execution)

* `Escribe los siguientes Shell scripts: - scripts/build.sh ... - scripts/run_demo.sh ... - scripts/lint.sh ...`
* `la documentation debe estar en ingles` (For scripts)
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp/scripts$ bash build.sh ... (Error: Kernel headers directory not found) ...`
* `I already run the commnad but I had the same error`
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp/scripts$ uname -r ... (diagnostic output) ...`
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp/scripts$ sudo apt-get update ... (Error: Unable to locate package) ...`
* `Esta versión de build.sh solo va a funcionar cuando tenga WSL, dame otra versión que funcione en Ubuntu normal`
* `ajusta run_demo.sh para que este en ingles`
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp/scripts$ dash run_demo.sh ... (Error: Bad substitution) ...`
* `(.venv) omarm@LAPTOP-2MTFKBQ8:~/WSL2-Linux-Kernel/simtemp/scripts$ bash run_demo.sh ... (Error: must be run with root) ...`
* `Todo el script debe estar en ingles` (For lint script)
* `[Image] un amigo probo el sh en su maquina con Linux y tuvo el siguiente error` (Error: cc: not found)
* `[Image] ahora tiene este error` (Error: gcc-12: not found)
* `crea el archivo TESTPLAN.md con base a la siguiente descripción de tests ... (TP1-TP5 y TS1 requirements) ...`
* `Add a test TP6 to test de modes, first set the ramp mode ... (Test noisy and ramp modes) ...`
* `dame el texto del test plan en el chat, en formato md`
* `modifica el código de test_mode.py para incluir los test que tiene TPx (Test Python) ... También modifica el script run_demo.sh par que incluya los test que inician con ID TS (Test script) ...`
* `dame el código del run_demo.sh`
* `vamos a midificar el TP1, Python es muy lento para leer, solo vamos a validar el periodo de muestreo con la diferencia en el valor del status. ...`
* `ajusta run_demo.sh para que corra el test desde main.py`
* `adjust TP1 test description with the new test P1 Python implementation: ... (old TP1 description) ...`
* `Dame solo la nueva descripción del TP1 en formato md`
* `ponlo en un formato que pueda copiar y pegar directo en el markdown` (For TP1 description)
* `Crea un archivo AI_NOTES.md con todos los prompts utilizados en esta conversación para el diseño del driver y la aplicación.` (Previous prompt)
* `Genera un archivo AI_NOTES.md con los promts utilizados en esta conversación desde el primer comentario. Podrías catalogarlos en los relacionados a la creación del driver, a la creación de la aplicación, la creación de script y pruebas` (Previous prompt)
* `los promts dejalos tal como en el idioma que van solo el documento hazlo en ingles, y pon una nota que la lista de promts esta incompleta, que se perdieron los promts mas antiguos de la conversación con la IA` (This current prompt)