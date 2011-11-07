In the book Programming Applications for Microsoft Windows (Fourth
Edition) by Jeffrey Richter, on page 794 he gives a technique for DLL
injection called "Injecting Code with CreateProcess". These are the
basic steps he outlines:

1. Have your process spawn the child process in a suspended state.

2. Retrieve the primary thread's starting memory address from the .exe
module's file header.

3. Save the machine instructions at this address.

4. Force some hand-coded machine instructions into this address. The
instructions will call LoadLibrary() to load a DLL.

5. Resume the child process's primary thread so that this code executes.

6. Restore the original instructions back into the starting address.

7. Let the process continue executing from the starting address as if
nothing had happened.

Richter does not give code for this. However, the idea was implemented
by Steven Engelhardt (see http://deez.info/sengelha/code/win32-ldpreload/).
We're using his code and it works very nicely.

However, there's still a problem with this (and with all DLL injection
techniques): The only "hook" available for executing code in the
injected DLL is the DLL_PROCESS_ATTACH clause of DllMain().
Unfortunately there are many actions not allowed from DllMain. Here is
what MSDN says:

"Warning: There are serious limits on what you can do in a DLL entry
point. To provide more complex initialization, create an initialization
routine for the DLL. You can require applications to call the
initialization routine before calling any other routines in the DLL."
(Specific known restrictions include (a) you may not call LoadLibrary
from DllMain, (b) you may not start a thread in DllMain and (c) you may
not call ExitProcess. The practical effect of (a) is that only
functions within kernel32.dll may be used.)

Of course when injecting a DLL into code for which we do not have
source and which was not written to our API, there's no way to
tell the program to call the initialization routine as recommended.

My idea is to modify Engelhardt's code to call an additional function
("PreMain") after the return from LoadLibrary.  In other words we
already have working code to find the program entry point and modify it
to insert a function call, so why can't we insert two function calls?
I don't have the assembly-language skills to do this which is why I'm
contracting it out.

The tricky part I can think of is that the PreMain function will have
to reside in the injected DLL, which means the injection routine cannot
know its address before the DLL is mapped (unlike LoadLibrary which is
guaranteed to already be mapped). But it appears that DllMain can work
that out without breaking any of its rules.

In other words I believe the injection function (Inject) can allocate
space for two function calls and just leave the address blank (or
pointing to some error handling routine). Then the DllMain can come
along and poke the address of PreMain into the blank slot and return.
The main program will call PreMain next and it will not have to worry
about DllMain restrictions because everything will be mapped and
initialized by then.

As far as I can tell the code to insert the PreMain call can use much
of what's already in place; the only difference would be that the
function address to be inserted and its signature would be different,
and the allocation part would already be done.

The result would be kind of a ping-pong effect - the parent program
uses Inject to insert a call to LoadLibrary plus a blank slot, then
hands control off to the child program. As the child loads the new DLL,
DllMain will be called. It can find the PreMain function within its own
DLL and use the same Inject logic to re-find the entry point and this
time fill the blank slot with PreMain's address. Then DllMain finishes
and control returns to the entry point which now has a call to
PreMain.  PreMain can ignore all DllMain restrictions.

Speaking of signature, it couldn't be simpler:

	void PreMain(void)

Actually if possible it would be a nice bonus to pass in the same
parameters seen by main():

	void PreMain(int argc, char *argv[])

But that may not be possible.