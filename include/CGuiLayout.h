/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// GUI Layout class
// Created 30/5/02
// Jason Boettcher


#ifndef __CGUILAYOUT_H__
#define __CGUILAYOUT_H__


// layout event structure
class gui_event_t { public:
	int		iControlID;
	int		iEventMsg;

	CWidget	*cWidget;
};


// Errors
enum {
	ERR_OUTOFMEMORY=0,
	ERR_UNKNOWNPROPERTY,
	ERR_COULDNOTPARSE,
	ERR_EMPTYDOC,
	ERR_INVALIDROOT
};


// Widget types
enum {
	wid_Button=0,
	wid_Label,
	wid_Listview,
	wid_Scrollbar,
	wid_Slider,
	wid_Textbox,
	wid_Titlebutton,
	wid_Checkbox,
	wid_Inputbox,
	wid_Image,
	wid_Frame,
	wid_Animation
};


class CGuiLayout {
public:
	// Constructor
	CGuiLayout() {
		tEvent = new gui_event_t;
		cFocused = NULL;
		cWidgets = NULL;
		cMouseOverWidget = NULL;
		iCanFocus = true;
		iID = -1;
		//Initialize();
	}

	// Destructor
	~CGuiLayout() {
		if (tEvent)
			delete tEvent;
		tEvent = NULL;
	}


private:
	// Attributes

	CWidget		*cWidgets;
	gui_event_t	*tEvent;
	CWidget		*cFocused;
	CWidget		*cMouseOverWidget;

	int			iID;

	// Mouse button repeats
	int			nMouseButtons;
	float		fMouseNext[3];

	// Can we set focus to another widget?
	int			iCanFocus;



public:
	// Methods

	void		Initialize(int LayoutID = -1);

	bool		Build(void);

	void		Add(CWidget *widget, int id, int x, int y, int w, int h);
	CWidget		*getWidget(int id);
    void        removeWidget(int id);
	int			GetIdByName(char *Name);
	void		Error(int ErrorCode, char *Format, ...);

	gui_event_t	*Process(void);
	void		Draw(SDL_Surface *bmpDest);

	void		Shutdown(void);


	// Messaging
	DWORD		SendMessage(int iControl, int iMsg, DWORD Param1, DWORD Param2);
	DWORD		SendMessage(int iControl, int iMsg, const std::string& sStr, DWORD Param);
	DWORD		SendMessage(int iControl, int iMsg, std::string *sStr, DWORD Param);

	// Variables
	int			getID(void)		{ return iID; }
	void		setID(int _id)	{ iID = _id; }

};




#endif  //  __CGUILAYOUT_H__
