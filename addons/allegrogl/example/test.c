

#include <allegro.h>
#include <alleggl.h>

int main() {
	
	/* Initialize both Allegro and AllegroGL */
	if (allegro_init())
		return 1;

	if (install_allegro_gl())
		return 1;
	
	/* Tell Allegro we want to use the keyboard */
	install_keyboard();

	/* Suggest a good screen mode for OpenGL */
	allegro_gl_set(AGL_Z_DEPTH, 8);
	allegro_gl_set(AGL_COLOR_DEPTH, 16);
	allegro_gl_set(AGL_SUGGEST, AGL_Z_DEPTH | AGL_COLOR_DEPTH);

	/* Set the graphics mode */
	if (set_gfx_mode(GFX_OPENGL_WINDOWED, 640, 480, 0, 0)) {
		set_gfx_mode(GFX_TEXT, 0, 0, 0, 0);
		allegro_message("Unable to set graphic mode\n%s\n", allegro_error);
		return 1;
	}
	
	/* Clear the screen */
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/* Set up the view frustum */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glFrustum(-1.0, 1.0, -1.0, 1.0, 1, 60.0);

	/* Draw a quad using OpenGL */
	glBegin(GL_QUADS);
		glColor3ub(255, 255, 255);
		glVertex3f(-1, -1, -2);
		glVertex3f( 1, -1, -2);
		glVertex3f( 1,  1, -2);
		glVertex3f(-1,  1, -2);
	glEnd();
	
	/* Flip the backbuffer on screen */
	allegro_gl_flip();
	
	/* Wait for the user to press a key */
	readkey();
	
	/* Finished. */
	return 0;
}
END_OF_MAIN()  