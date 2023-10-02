#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <raylib.h>
#include <unistd.h>
#include <sys/stat.h>

#define RAYGUI_IMPLEMENTATION
#include "include/raygui.h"

#undef RAYGUI_IMPLEMENTATION
#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include "include/gui_window_file_dialog.h"

#define WIDTH  800
#define HEIGHT 400

typedef struct {
	float hours;
	float minutes;
	float seconds;
} Time;


void set_error(char* message) {
	DrawText(message, WIDTH/2.0, HEIGHT/2.0+300, 13, RED);
}


int GuiTextBox2(Rectangle bounds, char *text, int bufferSize, bool editMode) {
	#if !defined(RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN)
		#define RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN  40        // Frames to wait for autocursor movement
	#endif
	#if !defined(RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY)
		#define RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY      1        // Frames delay for autocursor movement
	#endif
	
	int result = 0;
	GuiState state = guiState;
	
	bool multiline = false;     // TODO: Consider multiline text input
	int wrapMode = GuiGetStyle(DEFAULT, TEXT_WRAP_MODE);
	
	Rectangle textBounds = GetTextBounds(TEXTBOX, bounds);
	int textWidth = GetTextWidth(text) - GetTextWidth(text + textBoxCursorIndex);
	int textIndexOffset = 0;    // Text index offset to start drawing in the box
	
	// Cursor rectangle
	// NOTE: Position X value should be updated
	Rectangle cursor = {
		textBounds.x + textWidth + GuiGetStyle(DEFAULT, TEXT_SPACING),
		textBounds.y + textBounds.height/2 - GuiGetStyle(DEFAULT, TEXT_SIZE),
		2,
		(float)GuiGetStyle(DEFAULT, TEXT_SIZE)*2
	};
	
	if (cursor.height >= bounds.height) cursor.height = bounds.height - GuiGetStyle(TEXTBOX, BORDER_WIDTH)*2;
	if (cursor.y < (bounds.y + GuiGetStyle(TEXTBOX, BORDER_WIDTH))) cursor.y = bounds.y + GuiGetStyle(TEXTBOX, BORDER_WIDTH);
	
	// Mouse cursor rectangle
	// NOTE: Initialized outside of screen
	Rectangle mouseCursor = cursor;
	mouseCursor.x = -1;
	mouseCursor.width = 1;
	
	// Auto-cursor movement logic
	// NOTE: Cursor moves automatically when key down after some time
	if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_DELETE)) autoCursorCooldownCounter++;
	else
	{
		autoCursorCooldownCounter = 0;      // GLOBAL: Cursor cooldown counter
		autoCursorDelayCounter = 0;         // GLOBAL: Cursor delay counter
	}
	
	// Blink-cursor frame counter
	//if (!autoCursorMode) blinkCursorFrameCounter++;
	//else blinkCursorFrameCounter = 0;
	
	// Update control
	//--------------------------------------------------------------------
	// WARNING: Text editing is only supported under certain conditions:
	if ((state != STATE_DISABLED) &&                // Control not disabled
		!GuiGetStyle(TEXTBOX, TEXT_READONLY) &&     // TextBox not on read-only mode
		!guiLocked &&                               // Gui not locked
		!guiSliderDragging &&                       // No gui slider on dragging
		(wrapMode == TEXT_WRAP_NONE))               // No wrap mode
	{
		Vector2 mousePosition = GetMousePosition();
	
		if (editMode)
		{
			state = STATE_PRESSED;
			
			// If text does not fit in the textbox and current cursor position is out of bounds,
			// we add an index offset to text for drawing only what requires depending on cursor
			while (textWidth >= textBounds.width)
			{
				int nextCodepointSize = 0;
				GetCodepointNext(text + textIndexOffset, &nextCodepointSize);
				
				textIndexOffset += nextCodepointSize;
				
				textWidth = GetTextWidth(text + textIndexOffset) - GetTextWidth(text + textBoxCursorIndex);
			}
			
			int textLength = (int)strlen(text);     // Get current text length
			int codepoint = GetCharPressed();       // Get Unicode codepoint
			if (multiline && IsKeyPressed(KEY_ENTER)) codepoint = (int)'\n';
			
			if (textBoxCursorIndex > textLength) textBoxCursorIndex = textLength;
			
			// Encode codepoint as UTF-8
			int codepointSize = 0;
			const char *charEncoded = CodepointToUTF8(codepoint, &codepointSize);
			
			// Add codepoint to text, at current cursor position
			// NOTE: Make sure we do not overflow buffer size
			if (((multiline && (codepoint == (int)'\n')) || (codepoint >= 32)) && ((textLength + codepointSize) < bufferSize) && (isdigit(*charEncoded)))
			{
				 // Move forward data from cursor position
				 for (int i = (textLength + codepointSize); i > textBoxCursorIndex; i--) text[i] = text[i - codepointSize];
				
				 // Add new codepoint in current cursor position
				 for (int i = 0; i < codepointSize; i++) text[textBoxCursorIndex + i] = charEncoded[i];
				
				 textBoxCursorIndex += codepointSize;
				 textLength += codepointSize;
				
				 // Make sure text last character is EOL
				 text[textLength] = '\0';
			}
	
			// Move cursor to start
			if ((textLength > 0) && IsKeyPressed(KEY_HOME)) textBoxCursorIndex = 0;
			
			// Move cursor to end
			if ((textLength > textBoxCursorIndex) && IsKeyPressed(KEY_END)) textBoxCursorIndex = textLength;
			
			// Delete codepoint from text, after current cursor position
			if ((textLength > textBoxCursorIndex) && (IsKeyPressed(KEY_DELETE) || (IsKeyDown(KEY_DELETE) && (autoCursorCooldownCounter >= RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN))))
			{
				autoCursorDelayCounter++;
			
				if (IsKeyPressed(KEY_DELETE) || (autoCursorDelayCounter%RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY) == 0)      // Delay every movement some frames
				{
					int nextCodepointSize = 0;
					GetCodepointNext(text + textBoxCursorIndex, &nextCodepointSize);
	
					// Move backward text from cursor position
					for (int i = textBoxCursorIndex; i < textLength; i++) text[i] = text[i + nextCodepointSize];
					
					textLength -= codepointSize;
					
					// Make sure text last character is EOL
					text[textLength] = '\0';
				}
			}
			
			// Delete codepoint from text, before current cursor position
			if ((textLength > 0) && (IsKeyPressed(KEY_BACKSPACE) || (IsKeyDown(KEY_BACKSPACE) && (autoCursorCooldownCounter >= RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN))))
			{
				autoCursorDelayCounter++;
			
				if (IsKeyPressed(KEY_BACKSPACE) || (autoCursorDelayCounter%RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY) == 0)      // Delay every movement some frames
				{
					int prevCodepointSize = 0;
					GetCodepointPrevious(text + textBoxCursorIndex, &prevCodepointSize);
			
					// Move backward text from cursor position
					for (int i = (textBoxCursorIndex - prevCodepointSize); i < textLength; i++) text[i] = text[i + prevCodepointSize];
					
					// Prevent cursor index from decrementing past 0
					if (textBoxCursorIndex > 0)
					{
						textBoxCursorIndex -= codepointSize;
						textLength -= codepointSize;
					}
					
					// Make sure text last character is EOL
					text[textLength] = '\0';
				}
			}
			
			// Move cursor position with keys
			if (IsKeyPressed(KEY_LEFT) || (IsKeyDown(KEY_LEFT) && (autoCursorCooldownCounter > RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN)))
			{
				autoCursorDelayCounter++;
			
				if (IsKeyPressed(KEY_LEFT) || (autoCursorDelayCounter%RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY) == 0)      // Delay every movement some frames
				{
					int prevCodepointSize = 0;
					GetCodepointPrevious(text + textBoxCursorIndex, &prevCodepointSize);
			
					if (textBoxCursorIndex >= prevCodepointSize) textBoxCursorIndex -= prevCodepointSize;
				}
			}
			else if (IsKeyPressed(KEY_RIGHT) || (IsKeyDown(KEY_RIGHT) && (autoCursorCooldownCounter > RAYGUI_TEXTBOX_AUTO_CURSOR_COOLDOWN)))
			{
				autoCursorDelayCounter++;
				
				if (IsKeyPressed(KEY_RIGHT) || (autoCursorDelayCounter%RAYGUI_TEXTBOX_AUTO_CURSOR_DELAY) == 0)      // Delay every movement some frames
				{
					int nextCodepointSize = 0;
					GetCodepointNext(text + textBoxCursorIndex, &nextCodepointSize);
					
					if ((textBoxCursorIndex + nextCodepointSize) <= textLength) textBoxCursorIndex += nextCodepointSize;
				}
			}
			
			// Move cursor position with mouse
			if (CheckCollisionPointRec(mousePosition, textBounds))     // Mouse hover text
			{
				float scaleFactor = (float)GuiGetStyle(DEFAULT, TEXT_SIZE)/(float)guiFont.baseSize;
				int codepoint = 0;
				int codepointSize = 0;
				int codepointIndex = 0;
				float glyphWidth = 0.0f;
				float widthToMouseX = 0;
				int mouseCursorIndex = 0;
			
				for (int i = textIndexOffset; i < textLength; i++)
				{
					codepoint = GetCodepointNext(&text[i], &codepointSize);
					codepointIndex = GetGlyphIndex(guiFont, codepoint);
			
					if (guiFont.glyphs[codepointIndex].advanceX == 0) glyphWidth = ((float)guiFont.recs[codepointIndex].width*scaleFactor);
					else glyphWidth = ((float)guiFont.glyphs[codepointIndex].advanceX*scaleFactor);
			
					if (mousePosition.x <= (textBounds.x + (widthToMouseX + glyphWidth/2)))
					{
						mouseCursor.x = textBounds.x + widthToMouseX;
						mouseCursorIndex = i;
						break;
					}
					
					widthToMouseX += (glyphWidth + (float)GuiGetStyle(DEFAULT, TEXT_SPACING));
				}
			
				// Check if mouse cursor is at the last position
				int textEndWidth = GetTextWidth(text + textIndexOffset);
				if (GetMousePosition().x >= (textBounds.x + textEndWidth - glyphWidth/2))
				{
					mouseCursor.x = textBounds.x + textEndWidth;
					mouseCursorIndex = strlen(text);
				}
			
				// Place cursor at required index on mouse click
				if ((mouseCursor.x >= 0) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
				{
					cursor.x = mouseCursor.x;
					textBoxCursorIndex = mouseCursorIndex;
				}
			}
			else mouseCursor.x = -1;
			
			// Recalculate cursor position.y depending on textBoxCursorIndex
			cursor.x = bounds.x + GuiGetStyle(TEXTBOX, TEXT_PADDING) + GetTextWidth(text + textIndexOffset) - GetTextWidth(text + textBoxCursorIndex) + GuiGetStyle(DEFAULT, TEXT_SPACING);
			//if (multiline) cursor.y = GetTextLines()
			
			// Finish text editing on ENTER or mouse click outside bounds
			if ((!multiline && IsKeyPressed(KEY_ENTER)) ||
				(!CheckCollisionPointRec(mousePosition, bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)))
			{
				textBoxCursorIndex = 0;     // GLOBAL: Reset the shared cursor index
				result = 1;
			}
		}
		else
		{
			if (CheckCollisionPointRec(mousePosition, bounds))
			{
				state = STATE_FOCUSED;
	
				if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
				{
					textBoxCursorIndex = (int)strlen(text);   // GLOBAL: Place cursor index to the end of current text
					result = 1;
				}
			}
		}
	}
	//--------------------------------------------------------------------
	
	// Draw control
	//--------------------------------------------------------------------
	if (state == STATE_PRESSED)
	{
		GuiDrawRectangle(bounds, GuiGetStyle(TEXTBOX, BORDER_WIDTH), GetColor(GuiGetStyle(TEXTBOX, BORDER + (state*3))), GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_PRESSED)));
	}
	else if (state == STATE_DISABLED)
	{
		GuiDrawRectangle(bounds, GuiGetStyle(TEXTBOX, BORDER_WIDTH), GetColor(GuiGetStyle(TEXTBOX, BORDER + (state*3))), GetColor(GuiGetStyle(TEXTBOX, BASE_COLOR_DISABLED)));
	}
	else GuiDrawRectangle(bounds, GuiGetStyle(TEXTBOX, BORDER_WIDTH), GetColor(GuiGetStyle(TEXTBOX, BORDER + (state*3))), BLANK);
	
	// Draw text considering index offset if required
	// NOTE: Text index offset depends on cursor position
	GuiDrawText(text + textIndexOffset, textBounds, GuiGetStyle(TEXTBOX, TEXT_ALIGNMENT), GetColor(GuiGetStyle(TEXTBOX, TEXT + (state*3))));
	
	// Draw cursor
	if (editMode && !GuiGetStyle(TEXTBOX, TEXT_READONLY))
	{
		//if (autoCursorMode || ((blinkCursorFrameCounter/40)%2 == 0))
		GuiDrawRectangle(cursor, 0, BLANK, GetColor(GuiGetStyle(TEXTBOX, BORDER_COLOR_PRESSED)));
	
		// Draw mouse position cursor (if required)
		if (mouseCursor.x >= 0) GuiDrawRectangle(mouseCursor, 0, BLANK, GetColor(GuiGetStyle(TEXTBOX, BORDER_COLOR_PRESSED)));
	}
	else if (state == STATE_FOCUSED) GuiTooltip(bounds);
	//--------------------------------------------------------------------
	
	return result;      // Mouse button pressed: result = 1
}


void zfill(char* num, int width, char* des) {
	int num_len = (int)strlen(num);
	if (num_len >= width) {
		strcpy(des, num);
		return;
	}
	int fill = width - num_len;
	char new[width+1];
	for (int i = 0; i < fill; i++) {
		new[i] = '0';
	}
	for (int i = 0; i < num_len; i++) {
		new[fill+i] = num[i];
	}
	new[width] = '\0';
	strcpy(des, new);
}


void set_time_display(float hours, float minutes, float seconds, char* time_display) {
	char hours_zfill[3];
	char minutes_zfill[3];
	char seconds_zfill[3];

	char hours_string[3];
	char minutes_string[3];
	char seconds_string[3];

	snprintf(hours_string, 3, "%d", (int)hours);
	snprintf(minutes_string, 3, "%d", (int)minutes);
	snprintf(seconds_string, 3, "%d", (int)seconds);

	zfill(hours_string, 2, hours_zfill);
	zfill(minutes_string, 2, minutes_zfill);
	zfill(seconds_string, 2, seconds_zfill);

	time_display[0] = hours_zfill[0];
	time_display[1] = hours_zfill[1];
	time_display[2] = ':';
	time_display[3] = minutes_zfill[0];
	time_display[4] = minutes_zfill[1];
	time_display[5] = ':';
	time_display[6] = seconds_zfill[0];
	time_display[7] = seconds_zfill[1];
	time_display[8] = '\0';
}


void set(char* hours_box_text, char* minutes_box_text, char* seconds_box_text, char* time_display, int* seconds_left) {
	int hours = atoi(hours_box_text);
	int minutes = atoi(minutes_box_text);
	int seconds = atoi(seconds_box_text);
	if (minutes > 59) {
		strcpy(minutes_box_text, "59");
		minutes = atoi(minutes_box_text);
	}
	if (seconds > 59) {
		strcpy(seconds_box_text, "59");
		seconds = atoi(seconds_box_text);
	}

	set_time_display(hours, minutes, seconds, time_display);
	*seconds_left = hours*60*60+minutes*60+seconds;
}


Time seperate(float all) {
	Time result = {0};

	result.hours = floor(floor(all/60)/60);
	result.seconds = all-((result.hours)*60*60)-((floor((all-((result.hours)*60*60))/60))*60);
	result.minutes = (all-((result.hours)*60*60)-result.seconds)/60;

	return result;
}


bool set_new_alarm_path(const char* settings_file_path, const char* file_path) {
	FILE *fptr;

	fptr = fopen(settings_file_path, "w");

	if (fptr == NULL) {
		return false;
	}

	fprintf(fptr, "%s", file_path);

	fclose(fptr);
	return true;
}


char* get_alarm_path_from_settings(const char* settings_file_path, size_t* path_size) {
	size_t size = 0;
	char* contents = malloc(size);

	FILE *fptr = fopen(settings_file_path, "r");

	if (fptr == NULL) {
		return "";
	}

	char ch;
	do { 
		contents = realloc(contents, ++size);
		ch = fgetc(fptr);
		if (ch == '\n') break;
		contents[size-1] = ch;
	} while (ch != EOF);
	contents[size-1] = '\0';
	*path_size = size;

	fclose(fptr);
	return contents;
}


int main() {
	const int height_offset = 50;

	const int text_boxes_width = 100;
	const int text_boxes_x = WIDTH/2.0-text_boxes_width/2.0;
	const int widgets_height = 25;
	const int distance_between_text_boxes = 20;
	const int distance_between_button_and_text_box = 20; // 40
	const int distance_between_text_box_and_button = 15;
	const int distance_between_buttons = 5;

	Rectangle one_minute_button_bounds = {
		220,
		height_offset,
		80,
		widgets_height
	};
	Rectangle two_minute_button_bounds = {
		220+80+10,
		height_offset,
		80,
		widgets_height
	};
	Rectangle three_minute_button_bounds = {
		220+80+80+10+10,
		height_offset,
		80,
		widgets_height
	};
	Rectangle five_minute_button_bounds = {
		220+80+80+80+10+10+10,
		height_offset,
		80,
		widgets_height
	};
	Rectangle ten_minute_button_bounds = {
		220,
		height_offset+widgets_height+10,
		80,
		widgets_height
	};
	Rectangle fifteen_minute_button_bounds = {
		220+80+10,
		height_offset+widgets_height+10,
		80,
		widgets_height
	};
	Rectangle thirty_minute_button_bounds = {
		220+80+80+10+10,
		height_offset+widgets_height+10,
		80,
		widgets_height
	};
	Rectangle one_hour_button_bounds = {
		220+80+80+80+10+10+10,
		height_offset+widgets_height+10,
		80,
		widgets_height
	};

	Rectangle seconds_box_bounds = {
		text_boxes_x,
		one_hour_button_bounds.y+widgets_height+distance_between_button_and_text_box,
		text_boxes_width,
		widgets_height,
	};
	Rectangle minutes_box_bounds = {
		text_boxes_x,
		one_hour_button_bounds.y+widgets_height+distance_between_button_and_text_box+widgets_height*1+distance_between_text_boxes*1,
		text_boxes_width,
		widgets_height,
	};
	Rectangle hours_box_bounds = {
		text_boxes_x,
		one_hour_button_bounds.y+widgets_height+distance_between_button_and_text_box+widgets_height*2+distance_between_text_boxes*2,
		text_boxes_width,
		widgets_height,
	};

	Rectangle confirm_button_bounds = {
		text_boxes_x,
		hours_box_bounds.y+hours_box_bounds.height+distance_between_text_box_and_button,
		text_boxes_width,
		widgets_height
	};
	Rectangle clear_button_bounds = {
		text_boxes_x,
		hours_box_bounds.y+hours_box_bounds.height+distance_between_text_box_and_button+widgets_height+distance_between_buttons,
		text_boxes_width,
		widgets_height
	};


	// --- Getting the settings_file_path
	const char* home_dir = getenv("HOME");
	size_t home_dir_len = strlen(home_dir);
	size_t glock_config_dir_size;
	char after_home_dir[15];

	strcpy(after_home_dir, (home_dir[home_dir_len-1] == '/') ? ".config/glock" : "/.config/glock");
	glock_config_dir_size = home_dir_len+strlen(after_home_dir);

	char glock_config_dir_path[glock_config_dir_size];
	strcpy(glock_config_dir_path, home_dir);
	strcat(glock_config_dir_path, after_home_dir);

	char* settings_file_name = "/settings";
	char settings_file_path[glock_config_dir_size+strlen(settings_file_name)];
	strcpy(settings_file_path, glock_config_dir_path);
	strcat(settings_file_path, settings_file_name);
	// --- End of Getting settings_file_path


	bool hours_box_edit_mode = false;
	char hours_box_text[3] = "";
	bool minutes_box_edit_mode = false;
	char minutes_box_text[3] = "";
	bool seconds_box_edit_mode = false;
	char seconds_box_text[3] = "";

	// char current_error[128] = "";
	// here we say 9 because the format is hh:mm:ss and of course there is \0
	char time_display[9];

	time_t start_time = -1;
	int seconds_left = 0;
	int spent = -1;

	char* temp_alarm_file_path = NULL;
	size_t path_size;
	if (access(settings_file_path, F_OK) == 0) {
		temp_alarm_file_path = get_alarm_path_from_settings(settings_file_path, &path_size);
		
		if (strcmp(temp_alarm_file_path, "") == 0) {
			printf("ERROR: could not get alarm path from settings file. make sure that the file exists at '%s'\n", settings_file_path);
			return 1;
		}
	} else {
		printf("ERROR: could not open settings file. make sure that the file exists at '%s'\n", settings_file_path);
		return 1;
	}

	char alarm_file_path[path_size];
	strcpy(alarm_file_path, temp_alarm_file_path);
	free(temp_alarm_file_path);

	if (access(alarm_file_path, F_OK) != 0) {
		printf("ERROR: could not open the alarm file path inside settings file. make sure that the path inside the settings file at '%s' is valid.\n", settings_file_path);
		return 1;
	}

	InitWindow(WIDTH, HEIGHT, "Glock");
	InitAudioDevice();
	SetTargetFPS(60);

	GuiWindowFileDialogState file_dialog_state = InitGuiWindowFileDialog(GetWorkingDirectory());

	const int font_size = 15;
	Font default_font = GetFontDefault();
	const float font_spacing = 2;
	
	Music alarm = LoadMusicStream(alarm_file_path);
	SetMusicVolume(alarm, 0.1f);
	PlayMusicStream(alarm);

	while (!WindowShouldClose()) {
		if (file_dialog_state.SelectFilePressed) {
			if (IsFileExtension(file_dialog_state.fileNameText, ".mp3")) {
				const char* file_path_to_load = TextFormat("%s" PATH_SEPERATOR "%s", file_dialog_state.dirPathText, file_dialog_state.fileNameText);
				file_dialog_state.SelectFilePressed = false;
				if (!set_new_alarm_path(settings_file_path, file_path_to_load)) {
					printf("ERROR: could not open the settings file at '%s'\n", settings_file_path);
					UnloadMusicStream(alarm);
					CloseAudioDevice();
					CloseWindow();
					return 1;
				}
				StopMusicStream(alarm);
				alarm = LoadMusicStream(file_path_to_load);
				SetMusicVolume(alarm, 0.1f);
				PlayMusicStream(alarm);
			}
		}

		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			Vector2 mouse_position = GetMousePosition();
			if (CheckCollisionPointRec(mouse_position, hours_box_bounds)) {
				hours_box_edit_mode = true;
				minutes_box_edit_mode = false;
				seconds_box_edit_mode = false;
			} else if (CheckCollisionPointRec(mouse_position, minutes_box_bounds)) {
				minutes_box_edit_mode = true;
				hours_box_edit_mode = false;
				seconds_box_edit_mode = false;
			} else if (CheckCollisionPointRec(mouse_position, seconds_box_bounds)) {
				seconds_box_edit_mode = true;
				hours_box_edit_mode = false;
				minutes_box_edit_mode = false;
			} else {
				hours_box_edit_mode = false;
				minutes_box_edit_mode = false;
				seconds_box_edit_mode = false;
			}
		}
		BeginDrawing();
		ClearBackground(DARKGRAY);

		GuiTextBox2(hours_box_bounds, hours_box_text, 3, hours_box_edit_mode);
		GuiTextBox2(minutes_box_bounds, minutes_box_text, 3, minutes_box_edit_mode);
		GuiTextBox2(seconds_box_bounds, seconds_box_text, 3, seconds_box_edit_mode);

		DrawTextEx(default_font, "Second(s)", (Vector2){WIDTH/2.0-100/2.0-85, seconds_box_bounds.y+5}, font_size, font_spacing, WHITE);
		DrawTextEx(default_font, "Minute(s)", (Vector2){WIDTH/2.0-100/2.0-85, minutes_box_bounds.y+5}, font_size, font_spacing, WHITE);
		DrawTextEx(default_font, "Hour(s)", (Vector2){WIDTH/2.0-100/2.0-85, hours_box_bounds.y+5}, font_size, font_spacing, WHITE);

		if (GuiButton(one_minute_button_bounds, "1m")) {
			strcpy(minutes_box_text, "1");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(two_minute_button_bounds, "2m")) {
			strcpy(minutes_box_text, "2");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(three_minute_button_bounds, "3m")) {
			strcpy(minutes_box_text, "3");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(five_minute_button_bounds, "5m")) {
			strcpy(minutes_box_text, "5");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(ten_minute_button_bounds, "10m")) {
			strcpy(minutes_box_text, "10");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(fifteen_minute_button_bounds, "15m")) {
			strcpy(minutes_box_text, "15");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(thirty_minute_button_bounds, "30m")) {
			strcpy(minutes_box_text, "30");
			strcpy(seconds_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (GuiButton(one_hour_button_bounds, "1h")) {
			strcpy(hours_box_text, "1");
			strcpy(seconds_box_text, "");
			strcpy(minutes_box_text, "");
		}
		if (GuiButton(clear_button_bounds, "Clear")) {
			strcpy(seconds_box_text, "");
			strcpy(minutes_box_text, "");
			strcpy(hours_box_text, "");
		}
		if (seconds_left == 0) {
			if (GuiButton(confirm_button_bounds, "Set")) {
				set(hours_box_text, minutes_box_text, seconds_box_text, time_display, &seconds_left);
				start_time = time(NULL);
				spent = time(NULL)-start_time;
			}
		} else {
			if (GuiButton(confirm_button_bounds, "Unset")) {
				if (IsMusicStreamPlaying(alarm)) {
					StopMusicStream(alarm);
				}
				seconds_left = 0;
				strcpy(time_display, "00:00:00");
			} else {
				if (spent < seconds_left) {
					int new_spent = time(NULL)-start_time;
					if (new_spent != spent) {
						spent = new_spent;
						Time seperated = seperate(seconds_left-spent);
						set_time_display(seperated.hours, seperated.minutes, seperated.seconds, time_display);
					}
				} else {
					// if (GetMusicTimePlayed(alarm) >= 5.0) {
					// 	StopMusicStream(alarm);
					// 	seconds_left = 0;
					// }
					if (!IsMusicStreamPlaying(alarm)) {
						PlayMusicStream(alarm);
					}
					UpdateMusicStream(alarm);
				}
			}
		}

		int font_size = 20;
		int font_length = MeasureText(time_display, font_size);
		DrawText(time_display, WIDTH/2.0-font_length/2.0, HEIGHT-40, font_size, WHITE);
		// DrawText(time_display, WIDTH/2.0, HEIGHT-40, font_size, WHITE);

		if (file_dialog_state.windowActive) GuiLock();

		if (GuiButton((Rectangle){10, 10, 102, 25}, GuiIconText(ICON_FILE_OPEN, "Change Alarm"))) {
			file_dialog_state.windowActive = true;
		}
		GuiUnlock();

		GuiWindowFileDialog(&file_dialog_state);
		// set_error(current_error);
		EndDrawing();
	}
	UnloadMusicStream(alarm);
	CloseAudioDevice();
	CloseWindow();
	return 0;
}
