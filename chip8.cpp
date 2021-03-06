#include "stdafx.h"
#include "chip8.h"

#include "nuklear.h"

bool Chip8::m_Keys[16]{};
std::map<char, char>  Chip8::m_Keymap = {
	{'1', '1'},
	{'2', '2'},
	{'3', '3'},
	{'4', 'C'},

	{'Q', '4'},
	{'W', '5'},
	{'E', '6'},
	{'R', 'D'},

	{'A', '7'},
	{'S', '8'},
	{'D', '9'},
	{'F', 'E'},

	{'Z', 'A'},
	{'x', '0'},
	{'C', 'B'},
	{'V', 'F'}
};

unsigned char chip8_fontset[80] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

Chip8::Chip8() 
	: m_I(0)
	, m_Pc(0)
	, m_Sp(0)
{
}

void Chip8::create_display()
{
	// Creating a vertex attribute array
	// Each time we will call AttribPointer -> information stored in vao
	glGenVertexArrays(1, &m_DisplayData.vao);
	glBindVertexArray(m_DisplayData.vao);

	float vertices[] = { -1.0f	, 1.0f, 0.0f,0.0f,
							1.0f	, 1.0f, 1.0f,0.0f,
							1.0f	,-1.0f, 1.0f,1.0f,
							-1.0f	,-1.0f, 0.0f,1.0f };
	glGenBuffers(1, &m_DisplayData.vbo);
	/* Makes the buffer the active buffer*/
	glBindBuffer(GL_ARRAY_BUFFER, m_DisplayData.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	CheckError();

	GLuint elements[] = {
		2,1,0,
		3,2,0
	};
	glGenBuffers(1, &m_DisplayData.ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_DisplayData.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	/** Compiling our shaders */
	const char* vertShaderFile = ReadFile("res/vert.vert");
	const char* fragShaderFile = ReadFile("res/frag.frag");
	if (vertShaderFile == nullptr || fragShaderFile == nullptr)
	{
		Logger::LogWarning("Failed to load shader files!");
		std::cin.get();
		exit(-1);
	}
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertShaderFile, NULL);
	glCompileShader(vertexShader);
	CheckCompiledShader(vertexShader);
	m_DisplayData.vertexShader = vertexShader;

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragShaderFile, NULL);
	glCompileShader(fragmentShader);
	CheckCompiledShader(fragmentShader);
	CheckError();
	m_DisplayData.fragmentShader = fragmentShader;

	/* Combining our shaders */
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	CheckError();
	m_DisplayData.shaderProgram = shaderProgram;

	/* Binding fragment shader*/
	glBindFragDataLocation(shaderProgram, 0, "outColor");
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);
	CheckError();

	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	// Also stores current bound VBO
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	CheckError();
	GLint uvAttrib = glGetAttribLocation(shaderProgram, "texcoord");
	glEnableVertexAttribArray(uvAttrib);
	glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));


	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	unsigned char pixels[64 * 32]{};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 64, 32, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	CheckError();
	m_DisplayData.texture = texture;
}

extern GLFWwindow* g_pWindow;
void Chip8::init()
{
	int Width, Height;
	GLFWmonitor* pMonitor = glfwGetWindowMonitor(g_pWindow);
	if (!pMonitor)
	{
		int cnt = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&cnt);

		for (int i = 0; i < cnt; ++i)
		{
			pMonitor = monitors[i];
			break;
		}
	}
	
	glfwGetMonitorPhysicalSize(pMonitor, &Width, &Height);

	const GLFWvidmode* mode = glfwGetVideoMode(pMonitor);
	m_DPI = mode->width / (Width / 25.4);

	Logger::LogInfo("Initializing Chip8!");

	// Chip-8 roms start at 0x200 in memory
	m_Pc = 0x200;
	m_I = 0;
	m_Sp = 0;

	memset(m_Gfx, 0, 64 * 32);
	memset(m_Memory, 0, 4096);
	memset(m_Stack, 0, 16);
	memset(m_Keys, 0, 16);
	memset(m_V, 0, 16);

	m_DelayTimer = 0;
	m_SoundTimer = 0;

	create_display();

	// Load our font set
	for (int i = 0; i < 80; ++i)
		m_Memory[i] = chip8_fontset[i];
}

void Chip8::load_rom(const std::string& romName)
{
	// open file in binary and fill memory from location 0x200
	std::ifstream game(romName, std::ios::binary | std::ios::ate);
	if (game.fail())
	{
		std::cout << "Failed to load our game" << std::endl;
		exit(-1);
	}
	unsigned int size = (unsigned int)game.tellg();
	game.seekg(0);

	char *buffer = new char[size];
	game.read(buffer, size);

	deinit();
	init();

	memcpy(&m_Memory[0x200], buffer, size);

	m_CurrentGame = romName;
}

void Chip8::save_state(const std::string& saveLocation)
{
	std::string s = __func__;
	Logger::LogWarning(s + " >> SAVE CHIP8: Not implemented!");
}

/* Emulator emulation cycle */
void Chip8::update(float dt)
{
	// fetch Opcode
	// Fetch 2 bytes -> merge
	unsigned short opCode = m_Memory[m_Pc] << 8 | m_Memory[m_Pc + 1];

	m_EmulateTimer -= dt;
	if (m_EmulateTimer < 0)
	{
		m_EmulateTimer = MAX_EMULATE_RATE;
		execute_op_code(opCode);
	}
	m_UpdateTimer -= dt;
	if (m_UpdateTimer < 0) {
		m_UpdateTimer = TIMER_RATE;
		update_cntrs();
	}


}

void Chip8::execute_op_code(unsigned short opCode)
{
	switch (opCode & 0xF000)
	{
	case 0x0000: { // Some opcodes need more information
		switch (opCode & 0x000F)
		{
		case 0x0000:// 0x00E0 Clear the screen
			memset(m_Gfx, 0, 64 * 32);
			m_DrawFlag = true;
			m_Pc += 2;
			break;
		case 0x000E: {
			// 0x00EE: Returns from Subroutine
			// Set program counter to address on top of stack and decrement
			--m_Sp;
			m_Pc = m_Stack[m_Sp];
			m_Pc += 2;
		}break;
		default:
			Logger::LogOpCode(opCode, "unknown operator code.");
			break;
		}
	}break;
	case 0x1000: {
		// JMP
		m_Pc = opCode & 0x0FFF;
	}break;
	case 0x2000: {
		// Call subroutine at NNN
		// Store our current Location in the stack
		m_Stack[m_Sp] = m_Pc;
		++m_Sp;
		// Increment our stack to not overwrite it
		// Jump to address NNN
		m_Pc = opCode & 0x0FFF;
	}break;
	case 0x3000: {
		// If Vx == to kk skip our program counter
		unsigned short kk = opCode & 0x00FF;
		unsigned short x = (opCode & 0x0F00) >> 8;

		m_Pc += 2;
		if (m_V[x] == kk)
			m_Pc += 2;

	}break;
	case 0x4000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		unsigned short kk = (opCode & 0x00FF);

		m_Pc += 2;
		if (m_V[x] != kk)
			m_Pc += 2;
	}break;
	case 0x5000: {
		switch (opCode & 0x000F)
		{
			// Skip if Vx == Vy
		case 0x0000:
		{
			unsigned short x = (opCode & 0x0F00) >> 8;
			unsigned short y = (opCode & 0x00F0) >> 4;
			if (m_V[x] == m_V[y])
				m_Pc += 2;
			m_Pc += 2;
		}break;
		// Skip if Vx > Vy
		case 0x0001:
		{
			unsigned short x = (opCode & 0x0F00) >> 8;
			unsigned short y = (opCode & 0x00F0) >> 4;
			if (m_V[x] > m_V[y])
				m_Pc += 2;

			m_Pc += 2;
		}break;
		}
	}break;
	case 0x6000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		unsigned short kk = (opCode & 0x00FF);
		m_V[x] = (unsigned char)kk;
		m_Pc += 2;
	}break;
	case 0x7000: {
		unsigned short nn = opCode & 0x00FF;
		unsigned short x = (opCode & 0x0F00) >> 8;
		m_V[x] = m_V[x] + nn;
		m_Pc += 2;
	}break;
	case 0x8000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		unsigned short y = (opCode & 0x00F0) >> 4;
		switch (opCode & 0x000F)
		{
		case 0x0000: {
			m_V[x] = m_V[y];
			m_Pc += 2;
		}break;
		case 0x0001: {
			m_V[x] = m_V[x] | m_V[y];
			m_Pc += 2;
		}break;
		case 0x0002: {
			m_V[x] = m_V[x] & m_V[y];
			m_Pc += 2;
		}break;
		case 0x0003: {
			m_V[x] = m_V[x] ^ m_V[y];
			m_Pc += 2;
		}break;
		case 0x0004: {// 0x8XY4 -> adds value VY to VX. Register VF is set to 1 when carry
			unsigned short addition = m_V[x] + m_V[y];

			m_V[0xF] = 0;
			if (addition > 255)
				m_V[0xF] = 1;

			m_V[x] = addition & 0xFF;
			m_Pc += 2;
		}break;
		case 0x0005: {
			if (m_V[x] > m_V[y])
				m_V[0xF] = 1;
			else
				m_V[0xF] = 0;

			m_V[x] = m_V[x] - m_V[y];
			m_Pc += 2;
		}break;
		case 0x0006: {
			m_V[x] = m_V[x] >> 1;
			m_V[0xF] = m_V[x] & 0x8;

			m_V[x] = m_V[x] / 2;
			m_Pc += 2;
		}break;
		case 0x0007: {
			m_V[0xF] = 0;
			if (m_V[y] > m_V[x])
				m_V[0xF] = 1;

			m_V[x] = m_V[y] - m_V[x];
			m_Pc += 2;
		}break;
		case 0x000E: {
			m_V[x] = m_V[x] << 1;
			m_V[0xF] = m_V[x] & 0x1;
			m_V[x] *= 2;

			m_Pc += 2;
		}break;
		default:
			Logger::LogOpCode(opCode, "unknown operator code.");
			break;
		}
	}break;
	case 0x9000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		unsigned short y = (opCode & 0x00F0) >> 4;

		/* Skip next instruction if the values compare*/
		m_Pc += 2;
		if (m_V[x] != m_V[y])
			m_Pc += 2;

	}break;
	case 0xA000: { // ANNN: Sets I to the address NNN
		m_I = opCode & 0x0FFF;
		m_Pc += 2;
	}break;
	case 0xB000: {
		m_Pc = (opCode & 0x0FFF) + m_V[0];
	}break;
	case 0xC000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		unsigned char random = rand() % 256;
		m_V[x] = random & (opCode & 0x00FF);
		m_Pc += 2;
	}break;
	case 0xD000: { // Draws a sprite
		// A sprite is always 8 pixels long ( 8 bits)
		unsigned short x = m_V[(opCode & 0x0F00) >> 8];
		unsigned short y = m_V[(opCode & 0x00F0) >> 4];
		unsigned short height = opCode & 0x000F;
		// Clear our VF
		m_V[0xF] = 0;
		for (int i = 0; i < height; i++)
		{
			unsigned char pixels = m_Memory[m_I + i];
			for (int j = 0; j < 8; ++j)
			{
				// traverse through each bit
				int pixelX = (x + j) % 64;
				int pixelY = (y + i) % 32;
				int oneDPos = pixelX + (pixelY * 64);

				unsigned char pos = (0x80 >> j);
				if ((pixels & pos) != 0)
				{
					if (m_Gfx[oneDPos] == 0xFF) {
						m_V[0xF] = 1;
					}
					m_Gfx[oneDPos] ^= 0xFF;
				}

			}
		}
		m_DrawFlag = true;
		m_Pc += 2;
	}break;
	case 0xE000: {
		unsigned short x = (opCode & 0x0F00) >> 8;
		switch (opCode & 0x00FF) {
		case 0x009E: {
			// Skips instruction if key with the value of vX is pressed
			// m_Pc += 4;
			m_Pc += 2;
			if (m_Keys[m_V[x]] != 0)
				m_Pc += 2;
		}break;
		case 0x00A1: {
			// Skips next instruction if key with value vX is not pressed
			// m_Pc += 4;
			m_Pc += 2;
			if (m_Keys[m_V[x]] == 0)
				m_Pc += 2;
		}break;
		}
	}break;
	case 0xF000: {
		switch (opCode & 0x00FF)
		{
		case 0x0007: {
			unsigned short x = (opCode & 0x0F00) >> 8;
			m_V[x] = m_DelayTimer;
			m_Pc += 2;
		}break;
		case 0x000A: {
			// Wait for key press -> Store value in Vx
			bool keyPressed = false;

			for (int i = 0; i < 16; ++i)
			{
				bool k = m_Keys[i];
				keyPressed = k;
				if (k)
				{
					unsigned short x = (opCode & 0x0F00) >> 8;
					m_V[x] = i;
					m_Pc += 2;
					break;
				}
			}
			// TODO: Implement Key wait
			// m_Pc += 2;
		}break;
		case 0x0015: {
			m_DelayTimer = m_V[(opCode & 0x0F00) >> 8];
			m_Pc += 2;
		}break;
		case 0x0018: {
			m_SoundTimer = m_V[(opCode & 0x0F00) >> 8];
			m_Pc += 2;
		}break;
		case 0x001E: {
			m_I += m_V[(opCode & 0x0F00) >> 8];
			m_Pc += 2;
		}break;
		case 0x0029: {
			m_I = m_V[(opCode & 0x0F00) >> 8] * 5;
			m_Pc += 2;
		}break;
		case 0x0033: {
			m_Memory[m_I] = m_V[(opCode & 0x0F00) >> 8] / 100;
			m_Memory[m_I + 1] = (m_V[(opCode & 0x0F00) >> 8] / 10) % 10;
			m_Memory[m_I + 2] = (m_V[(opCode & 0x0F00) >> 8] % 100) % 10;
			m_Pc += 2;
		}break;
		case 0x0055: {
			for (int i = 0; i < 16; ++i)
			{
				m_Memory[m_I + i] = m_V[i];
			}
			m_Pc += 2;
		}break;
		case 0x0065: {
			unsigned short idx = (opCode & 0x0F00) >> 8;
			for (int i = 0; i <= idx; ++i)
			{
				m_V[i] = m_Memory[m_I + i];
			}
			m_Pc += 2;
		}break;
		default:
			Logger::LogOpCode(opCode, "unknown operator code.");
		}
	}break;
	default:
		Logger::LogOpCode(opCode, "unknown operator code.");
		break;
	}
	if (m_LogOpCodes)
		Logger::LogOpCode(opCode);
}

void Chip8::update_cntrs()
{
	// update timers
	if (m_DelayTimer > 0)
		--m_DelayTimer;

	if (m_SoundTimer > 0)
	{
		if (m_SoundTimer == 1)
			printf("BEEP!\n");
		--m_SoundTimer;
	}
}

/* Debug GUI drawing */
void Chip8::on_gui(struct nk_context* ctx)
{
	nk_style_push_style_item(ctx, &ctx->style.window.fixed_background, nk_style_item_color(nk_rgba(0, 0, 0, 0)));
	if (nk_begin(ctx, "Main menu", nk_rect(0, 0, 1280, 35), 0))
	{
		nk_panel* layout = nk_window_get_panel(ctx);
		nk_menubar_begin(ctx);
		nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
		nk_layout_row_push(ctx, 100);
		if (nk_menu_begin_label(ctx, "Menu", NK_TEXT_LEFT, nk_vec2(120, 200))) {
			nk_layout_row_dynamic(ctx, 25, 1);
			if (nk_menu_item_label(ctx, "Reload", NK_TEXT_LEFT))
			{
				load_rom(m_CurrentGame);
			}
			nk_menu_end(ctx);
		}
		if (nk_menu_begin_label(ctx, "Windows", NK_TEXT_LEFT, nk_vec2(120, 200))) {
			nk_layout_row_dynamic(ctx, 25, 1);
			if (nk_menu_item_label(ctx, ((m_ShowFlags&ShowFlags_ShowRegisters) ? "Registers*" : "Registers"), NK_TEXT_LEFT)) {
				m_ShowFlags = (m_ShowFlags ^ ShowFlags_ShowRegisters);
			}
			if (nk_menu_item_label(ctx, ((m_ShowFlags& ShowFlags_ShowSettings) ? "Settings*" : "Settings"), NK_TEXT_LEFT)) {
				m_ShowFlags = (m_ShowFlags ^ ShowFlags_ShowSettings);
			}
			if (nk_menu_item_label(ctx, ((m_ShowFlags& ShowFlags_ShowStacks) ? "Stack*" : "Stack"), NK_TEXT_LEFT)) {
				m_ShowFlags = (m_ShowFlags ^ ShowFlags_ShowStacks);
			}
			if (nk_menu_item_label(ctx, ((m_ShowFlags& ShowFlags_ShowMemory) ? "Memory*" : "Memory"), NK_TEXT_LEFT)) {
				m_ShowFlags = (m_ShowFlags ^ ShowFlags_ShowMemory);
			}
			nk_menu_end(ctx);
		}
		nk_layout_row_end(ctx);
		nk_menubar_end(ctx);
	}
	nk_end(ctx);
	nk_style_pop_style_item(ctx);

	if (nk_begin(ctx, "Log", nk_rect(0, 640 - 25, 1280, 30), 0))
	{
		nk_layout_row_dynamic(ctx, 10, 1);
		nk_label(ctx, Logger::GetInstance().GetLog().rbegin()->c_str(), NK_LEFT);
	}
	nk_end(ctx);

	/* Drawing our registers window*/
	if (m_ShowFlags & ShowFlags_ShowRegisters)
	{
		if (nk_begin(ctx, "Registers", nk_rect(640, 110, 100, 0xF * 21 + 5), NK_WINDOW_CLOSABLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE))
		{
			for (int i = 0; i < 0xF; ++i)
			{
				nk_layout_row_begin(ctx, nk_layout_format::NK_DYNAMIC, 14, 2);
				{
					nk_layout_row_push(ctx, 0.25f);
					std::string rVal = "V" + std::to_string(i) + ":";
					nk_label(ctx, rVal.c_str(), NK_TEXT_LEFT);
					nk_layout_row_push(ctx, 0.7f);
					std::string v = std::to_string(get_registers()[i]);
					nk_label(ctx, v.c_str(), NK_TEXT_RIGHT);

				}
				nk_layout_row_end(ctx);
			}

		}
		else m_ShowFlags &= ~ShowFlags_ShowRegisters;
		nk_end(ctx);
	}

	/* Drawing our settings window */
	if (m_ShowFlags & ShowFlags_ShowSettings)
	{
		float w = 270;
		if (nk_begin(ctx, "Settings", nk_rect(0, 60, 300, 300), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE))
		{
			nk_layout_row_begin(ctx, NK_STATIC, 25, 3);

			nk_layout_row_push(ctx, 0.2f*w);

			nk_label(ctx, "EmuSpeed: ", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, 0.6f*w);
			float v = 1 / MAX_EMULATE_RATE;
			nk_slider_float(ctx, 2, &v, 1200, 1.0f);
			MAX_EMULATE_RATE = 1.0f / v;
			nk_layout_row_push(ctx, 0.1f*w);
			nk_label(ctx, std::to_string(v).c_str(), NK_TEXT_RIGHT);

			nk_layout_row_end(ctx);

			nk_layout_row_begin(ctx, NK_DYNAMIC, 25, 2);
			nk_layout_row_push(ctx, 0.2f);
			nk_label(ctx, "Log: ", NK_TEXT_LEFT);
			nk_layout_row_push(ctx, 0.8f);
			m_LogOpCodes = !nk_check_label(ctx, "", !m_LogOpCodes);
			nk_layout_row_end(ctx);
		}
		else  m_ShowFlags &= ~ShowFlags_ShowSettings;
		nk_end(ctx);

	}
	if (m_ShowFlags & ShowFlags_ShowStacks)
	{
		float w = 270;
		if (nk_begin(ctx, "Stack", nk_rect(0, 60, w, 300), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
		{
			/* Iterator through the stack and show it */
			unsigned short opcode = m_Memory[m_Pc] << 8 | m_Memory[m_Pc + 1];
			nk_layout_row_begin(ctx, NK_DYNAMIC, 14, 3);
			{
				nk_layout_row_push(ctx, 0.33f);
				nk_label(ctx, ToHex(m_Pc).c_str(), NK_TEXT_LEFT);
				nk_label(ctx, ToHex(opcode).c_str(), NK_TEXT_LEFT);
				nk_label(ctx, "TODO", NK_TEXT_LEFT);
			}
			nk_layout_row_end(ctx);
			for (size_t i = 0; i < std::size(m_Stack); ++i)
			{
				unsigned short address = m_Stack[i];
				unsigned short opcode = m_Memory[address];
				nk_layout_row_begin(ctx, NK_DYNAMIC, 14, 3);
				{
					nk_layout_row_push(ctx, 0.33f);
					nk_label(ctx, ToHex(address).c_str(), NK_TEXT_LEFT);
					nk_label(ctx, ToHex(opcode).c_str(), NK_TEXT_LEFT);
					nk_label(ctx, "TODO", NK_TEXT_LEFT);
				}
				nk_layout_row_end(ctx);
			}
		}
		else  m_ShowFlags &= ~ShowFlags_ShowStacks;
		nk_end(ctx);
	}
	if (m_ShowFlags & ShowFlags_ShowMemory)
	{
		float w = 270;
		if (nk_begin(ctx, "Memory", nk_rect(0, 60, w, 300), NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
		{
			nk_list_view v;
			nk_layout_row_dynamic(ctx, 250, 1);
			nk_list_view_begin(ctx, &v, "MemoryInstructions", 0, 12, 3583);
			nk_layout_row_dynamic(ctx, 11, 1);
			for (int i = 0; i < v.count; i++)
			{
				// Extract opcode
				unsigned short OpCode = m_Memory[m_Pc + i] << 8 | m_Memory[m_Pc + 1 + i];
				nk_label(ctx, ToHex(OpCode).c_str(), NK_TEXT_CENTERED);
			}

			nk_list_view_end(&v);
		}
		else  m_ShowFlags &= ~ShowFlags_ShowMemory;
		nk_end(ctx);
	}
}

/* Emulator drawing */
void Chip8::on_draw(GLFWwindow* pWindow) const
{
	int w, h;
	glfwGetWindowSize(pWindow, &w, &h);
	glViewport(15, 15, w - 30, h - 30);

	if (m_DrawFlag)
	{
		// Update our texture
		glBindTexture(GL_TEXTURE_2D, m_DisplayData.texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 32, GL_RED, GL_UNSIGNED_BYTE, get_gfx());
		// Draw our texture
		m_DrawFlag = false;
	}

	double s = glfwGetTime();
	glUseProgram(m_DisplayData.shaderProgram);
	glBindTexture(GL_TEXTURE_2D, m_DisplayData.texture);
	glBindVertexArray(m_DisplayData.vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	double dur = glfwGetTime() - s;
	//Logger::LogInfo("Draw: " + std::to_string(dur));
}

/* Cleanup */
void Chip8::deinit()
{
	Free(m_DisplayData);
}

/* Static input binding */
void Chip8::set_keys(GLFWwindow* window, int key, int scannode, int action, int mods)
{
	/* Map keys to hex keypad */
	// Translate pressed key into keypad 
	if (m_Keymap.find(key) != m_Keymap.end())
	{
		char val = m_Keymap[key];
		unsigned int value;
		std::stringstream ss;
		ss << val;
		ss >> std::hex >> value;
		if (value >= 0 && value < 16)
		{
			if (action == GLFW_PRESS) {
				m_Keys[value] = true;
			}
			if (action == GLFW_RELEASE) {
				m_Keys[value] = false;
			}
		}
	}
}
