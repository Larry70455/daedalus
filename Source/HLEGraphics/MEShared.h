#ifndef HLEGRAPHICS_MESHARED_H_
#define HLEGRAPHICS_MESHARED_H_

//*****************************************************************************
// Data written by the display list front end (DAEDALUS_ME_GRAPHICS)
//
// The front end is meant to run on the Media Engine while the main CPU
// carries on with the game. Each CPU has its own data cache, and when the
// Media Engine finishes a job it writes back every 64 byte cache line it
// changed, whole. A variable the main CPU writes in the same line in the
// meantime would be lost (and the main CPU's copy of the line could
// overwrite the Media Engine's results). So every variable the front end
// writes is kept in cache lines of its own: SMELine<T> is 64 byte aligned,
// and its size is a whole number of lines. The old names stay usable
// through a #define next to each variable (name_.v).
//*****************************************************************************
template< typename T >
struct alignas(64) SMELine
{
	T	v;
};

#endif // HLEGRAPHICS_MESHARED_H_
