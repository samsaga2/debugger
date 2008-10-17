#ifndef VDPDATASTORE_H
#define VDPDATASTORE_H

#include <QObject>
#include "OpenMSXConnection.h"
#include "CommClient.h"
#include "Settings.h"

#include "SimpleHexRequest.h"
class BigHexRequest;

class VDPDataStore : public QObject, public SimpleHexRequestUser
{
	Q_OBJECT
public:
	VDPDataStore();
	~VDPDataStore();

	static VDPDataStore& instance();

	unsigned char* getVramPointer();
	unsigned char* getPalettePointer();
	unsigned char* getRegsPointer();
	unsigned char* getStatusRegsPointer();
	bool old_version;	// TODO these should be private but then
	bool got_version;	// VDPDataStoreVersionCheck needs to be 'friend'
				// so this is simpler for this WIP :-)

private:
	unsigned char* vram;
	unsigned char* palette;
	unsigned char* regs;
	unsigned char* statusRegs;

public slots:
	void refresh();

signals:
        void dataRefreshed(); // The refresh got the new data

	/** This might become handy later on, for now we only need the dataRefreshed
	 *
        void dataChanged(); //any of the contained data has changed
        void vramChanged(); //only the vram changed
        void paletteChanged(); //only the palette changed
        void regsChanged(); //only the regs changed
        void statusRegsChanged(); //only the regs changed
	*/

protected:
	virtual void DataHexRequestReceived();

};

#endif /* VDPDATASTORE_H */