// Header files
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#include "gcode.h"

using namespace std;


// Definitions
enum filamentTypes {PLA, ABS, HIPS, OTHER};


// Global variables
int fd = -1;
string firmwareVersion;
string serialNumber;
double backRightOffset;
double backLeftOffset;
double frontRightOffset;
double frontLeftOffset;
double bedHeightOffset;
double backlashX;
double backlashY;
double backRightOrientation;
double backLeftOrientation;
double frontRightOrientation;
double frontLeftOrientation;
uint8_t status;
string folderLocation;

uint8_t temperature = 215;
filamentTypes filament = PLA;
double sizeX, sizeY, sizeZ;

bool useBasicPreparation = false;
bool useWaveBonding = false;
bool useThermalBonding = false;
bool useBedCompensation = false;
bool useBacklashCompensation = false;
bool useFeedRateConversion = false;


// Function prototypes

/*
Name: Connect to printer
Purpose: Connects to the printer
*/
bool connectToPrinter();

/*
Name: Send Data
Purpose: Sends ASCII data to the device
*/
bool sendDataAscii(const char *data);

/*
Name: Send Data
Purpose: Sends binary data to the device
*/
bool sendDataBinary(const char *data);

/*
Name: Get Response
Purpose: Receives response from the device
*/
string getResponse();

/*
Name: Get Print Dimensions
Purpose: Calculates max X, Y, and Z sizes of the print
*/
bool getPrintDimensions();

/*
Name: Basic Preparation Preprocessor
Purpose: Adjusts the input file to remove unwanted commands and adds printer specific starting and ending command sequences
*/
bool basicPreparationPreprocessor();

/*
Name: Wave Bonding Preprocessor
Purpose: Adjusts the input file to incoporate wave bonding
*/
bool waveBondingPreprocessor();

/*
Name: Thermal Bonding Preprocessor
Purpose: Adjusts the input file to incoporate thermal bonding
*/
bool thermalBondingPreprocessor();

/*
Name: Bed Compensation Preprocessor
Purpose: Adjusts the input file to incoporate bed compensation
*/
bool bedCompensationPreprocessor();

/*
Name: Backlash Compensation Preprocessor
Purpose: Adjusts the input file to incoporate backlash compensation
*/
bool backlashCompensationPreprocessor();

/*
Name: Feed Rate Conversion Preprocessor
Purpose: Adjusts the input file to incoporate feed rate conversion
*/
bool feedRateConversionPreprocessor();


// Main function
int main(int argc, char *argv[]) {

	// Initialize variables
	string response, line;
	fstream processedFile;
	ifstream input;
	char character;
	uint64_t totalLines = 0, lineCounter = 0;
	
	// Check if a file is provided
	if(argc >= 2) {
	
		// Check if file doesn't exists
		input.open(argv[1]);
		if(!input.good()) {
		
			// Display error
			cout << "Input file doesn't exist" << endl;
			return 0;
		}
	}
	
	// Wait for device to be connected
	while(!connectToPrinter()) {
		cout << "M3D not detected" << endl;
		usleep(500000);
	}
	
	// Display message
	cout << "Connected to M3D" << endl;
	cout << "Initializing the device" << endl;
	
	// Put device into firmware mode
	while(1) {
		sendDataAscii("M115");
		while(read(fd, &character, 1) == -1);
		if(character == 'e')
			break;
		sendDataAscii("Q");
		while(!connectToPrinter());
		usleep(50000);
	}
	
	// Get device info
	sendDataBinary("M115");
	response = getResponse();
	
	// Set firmware and serial number 
	firmwareVersion = response.substr(response.find("FIRMWARE_VERSION:") + 17, response.find(" ", response.find("FIRMWARE_VERSION:")) - response.find("FIRMWARE_VERSION:") - 17);
	serialNumber = response.substr(response.find("SERIAL_NUMBER:") + 14);
	
	// Get bed offsets
	sendDataBinary("M578");
	response = getResponse();
	
	// Set bed offsets
	backRightOffset = stod(response.substr(response.find("BRO:") + 4, response.find(" ", response.find("BRO:")) - response.find("BRO:") - 4));
	backLeftOffset = stod(response.substr(response.find("BLO:") + 4, response.find(" ", response.find("BLO:")) - response.find("BLO:") - 4));
	frontRightOffset = stod(response.substr(response.find("FRO:") + 4, response.find(" ", response.find("FRO:")) - response.find("FRO:") - 4));
	frontLeftOffset = stod(response.substr(response.find("FLO:") + 4, response.find(" ", response.find("FLO:")) - response.find("FLO:") - 4));
	bedHeightOffset = stod(response.substr(response.find("ZO:") + 3));

	// Get backlash
	sendDataBinary("M572");
	response = getResponse();
	
	// Set backlash values
	backlashX = stod(response.substr(response.find("BX:") + 3, response.find(" ", response.find("BX:")) - response.find("BX:") - 3));
	backlashY = stod(response.substr(response.find("BY:") + 3));
	
	// Get bed orientation
	sendDataBinary("M573");
	response = getResponse();
	
	// Set bed orientation
	backRightOrientation = stod(response.substr(response.find("BR:") + 3, response.find(" ", response.find("BR:")) - response.find("BR:") - 3));
	backLeftOrientation = stod(response.substr(response.find("BL:") + 3, response.find(" ", response.find("BL:")) - response.find("BL:") - 3));
	frontLeftOrientation = stod(response.substr(response.find("FL:") + 3, response.find(" ", response.find("FL:")) - response.find("FL:") - 3));
	frontRightOrientation = stod(response.substr(response.find("FR:") + 3));
	
	// Get status
	sendDataBinary("M117");
	response = getResponse();
	
	// Set status
	status = stod(response.substr(response.find("S:") + 2));
	
	// Display device information
	cout << endl << "Firmware: " << firmwareVersion << endl;
	cout << "Serial Number: " << serialNumber << ' ';
	if(serialNumber.substr(0, 2) == "BK")
		cout << "Black";
	else if(serialNumber.substr(0, 2) == "SL")
		cout << "Silver";
	else if(serialNumber.substr(0, 2)  == "BL")
		cout << "Blue";
	else if(serialNumber.substr(0, 2) == "GR")
		cout << "Green";
	else if(serialNumber.substr(0, 2) == "OR")
		cout << "Orange";
	cout << endl << "Back Right Offset: " << fixed << setprecision(4) << backRightOffset << endl;
	cout << "Back Left Offset: " << backLeftOffset << endl;
	cout << "Front Right Offset: " << frontRightOffset << endl;
	cout << "Front Left Offset: " << frontLeftOffset << endl;
	cout << "Bed Height Offset: " << bedHeightOffset << endl;
	cout << "Backlash X: " << backlashX << endl;
	cout << "Backlash Y: " << backlashY << endl;
	cout << "Back Right Orientation: " << backRightOrientation << endl;
	cout << "Back Left Orientation: " << backLeftOrientation << endl;
	cout << "Front Right Orientation: " << frontRightOrientation << endl;
	cout << "Front left Orientation: " << frontLeftOrientation << endl;
	cout << "Status: 0x" << hex << uppercase << static_cast<unsigned int>(status) << endl;
	cout << "Z Calibration Valid: " << (status & 0x02 ? "Yes" : "No") << endl;
	cout << endl << "Device is ready" << endl;
	
	// Check if a file was provided
	if(argc >= 2) {
	
		// Create temporary folder
		folderLocation = mkdtemp(const_cast<char *>(static_cast<string>("/tmp/m3d-XXXXXX").c_str()));
		
		// Check if creating processed file was successful
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::app);
		if(processedFile.good()) {
		
			// Display message
			cout << "Processing " << argv[1] << endl;
			
			// Read in input file
			processedFile << input.rdbuf();
			processedFile.close();

			// Get print dimensions
			getPrintDimensions();
			
			// Use preparation preprocessor if set
			if(useBasicPreparation)
				basicPreparationPreprocessor();
			
			// Use wave bonding preprocessor if set
			if(useWaveBonding)
				waveBondingPreprocessor();
			
			// Use thermal bonding preprocessor if set
			if(useThermalBonding)
				thermalBondingPreprocessor();
			
			// Use bed compensation preprocessor if set
			if(useBedCompensation)
				bedCompensationPreprocessor();
			
			// Use backlash compensation preprocessor if set
			if(useBacklashCompensation)
				backlashCompensationPreprocessor();
			
			// Use feed rate conversion proprocessor if set
			if(useFeedRateConversion)
				feedRateConversionPreprocessor();
			
			// Determine the number of command lines in the fully processed file
			processedFile.open(folderLocation + "/output.gcode", ios::in | ios::binary);
			while(processedFile.peek() != EOF) {
				getline(processedFile, line);
				totalLines++;
			}
			processedFile.clear();
			processedFile.seekg(0, ios::beg);
		
			// Display message
			cout << "Starting print" << endl;
		
			// Go through file
			while(processedFile.peek() != EOF) {

				// Get line
				getline(processedFile, line);

				// Display percent complete
				lineCounter++;
				cout << dec << lineCounter << '/' << totalLines << ' ' << static_cast<float>(lineCounter) / totalLines << '%' << endl;

				// Send line to the device
				do {
					
					// Get next command if line didn't contain valid g-code
					if(!sendDataBinary(line.c_str())) {
					
						cout << "Failed to parse " << line << endl << endl;
						break;
					}
					
					// Display command
					cout << "Send: " << line << endl;
					
					// Get valid response
					response = getResponse();
					if(line.substr(0, 2) == "G0" && response == "Info:Too small")
						response = getResponse();
					
					// Display response
					cout << "Receive: " << response << endl << endl;
				} while(response.substr(0, 2) != "ok");
			
				// Close processed file
				processedFile.close();
			}

			// Delete processed file
			unlink((folderLocation + "/output.gcode").c_str());
			
			// Display message
			cout << "Print finished" << endl;
		}

		// Delete temporary folder
		rmdir(folderLocation.c_str());
	}
	
	// Otherwise
	else {
	
		// Display message
		cout << "Entering manual G-code mode" << endl << "Enter 'quit' to exit" << endl;
		
		// Loop forever
		while(1) {
	
			// Get g-code from user
			cout << "Enter command: ";
			getline(cin, line);
			
			// End if requested
			if(line == "quit")
				break;
		
			// Send g-code to device
			do {
				
				// Get next command if line didn't contain valid g-code
				if(!sendDataBinary(line.c_str())) {
				
					cout << "Failed to parse command" << endl << endl;
					break;
				}
				
				// Display command
				cout << "Send: " << line << endl;
				
				// Get valid response
				response = getResponse();
				if(line.substr(0, 2) == "G0" && response == "Info:Too small")
					response = getResponse();
				
				// Display response
				cout << "Receive: " << response << endl << endl;
			} while(response.substr(0, 2) != "ok" && response.substr(0, 2) != "T:");
		}
	}
	
	// Close file descriptor
	close(fd);
	
	// Return 0
	return 0;
}


// Supporting function implementation
bool connectToPrinter() {

	// Initialize variables
        termios settings;
        
        // Close file descriptor if open
        if(fd != -1)
        	close(fd);
        
        // Check if opening device was successful
	if((fd = open("/dev/micro_m3d", O_RDWR | O_NONBLOCK)) != -1) {
      
		// Set serial protocol to 8n1 with 921600 baud rate
		memset(&settings, 0, sizeof(settings));
		settings.c_iflag = 0;
		settings.c_oflag = 0;
		settings.c_cflag= CS8 | CREAD | CLOCAL;
		settings.c_lflag = 0;
		settings.c_cc[VMIN] = 1;
		settings.c_cc[VTIME] = 5;
		cfsetospeed(&settings, B921600);
		cfsetispeed(&settings, B921600);

		// Apply settings
		tcsetattr(fd, TCSAFLUSH, &settings);
		tcdrain(fd);

		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool sendDataAscii(const char *data) {

	// Initialize variables
	bool returnValue;

	// Send data to the device
	tcflush(fd, TCIOFLUSH);
	returnValue = write(fd, data, strlen(data)) != -1;
	tcdrain(fd);
	
	// Return value
	return returnValue;
}

bool sendDataBinary(const char *data) {

	// Initialize variables
	bool returnValue;
	Gcode gcode;
	
	// Check if line was successfully parsed
	if(gcode.parseLine(data)) {
	
		// Send binary request to the device
		tcflush(fd, TCIOFLUSH);
		returnValue = write(fd, gcode.getBinary().data(), gcode.getBinary().size()) != -1;
		tcdrain(fd);
		
		// Return value
		return returnValue;
	}
	
	// Return false
	return false;
}

string getResponse() {

	// Initialize variables
	string response;
	char character;
	
	// Get response
	do {
		while(read(fd, &character, 1) == -1);
		response.push_back(character);
	} while(character != '\n');
	
	// Remove newline character from response
	response.pop_back();
	
	// Return response
	return response;
}

bool getPrintDimensions() {

	// Initialize variables
	string line;
	Gcode gcode;
	fstream file(folderLocation + "/output.gcode", ios::in | ios::binary);
	double minX = 10000, maxX = 0;
	double minY = 10000, maxY = 0;
	double minZ = 10000, maxZ = 0;
	double localX = 0, localY = 0, localZ = 0, localE = 0, commandX, commandY, commandZ, commandE;
	bool relativeMode = false, positiveExtrusion;
	
	// Check if file was opened successfully
	if(file.good()) {
	
		// Go through file
		while(file.peek() != EOF) {
	
			// Read in line
			getline(file, line);
			
			// Check if line was parsed successfully and it's a G command
			if(gcode.parseLine(line) && gcode.hasValue('G')) {
			
				// Check what parameter is associated with the command
				switch(stoi(gcode.getValue('G'))) {
				
					case 0:
					case 1:
					
						// Clear positive extruding
						positiveExtrusion = false;
						
						// Check if extruding
						if(gcode.hasValue('E')) {
						
							// Get E value of the command
							commandE = stod(gcode.getValue('E'));
							
							// Set positive extrusion based on adjusted extrusion value
							if(relativeMode) {
								positiveExtrusion = commandE > 0;
								localE += commandE;
							}
							
							else {
								positiveExtrusion = commandE > localE;
								localE = commandE;
							}
						}
						
						// Check if positive extruding
						if(positiveExtrusion) {
						
							// Set minimums and maximums
							minX = minX < localX ? minX : localX;
							maxX = maxX > localX ? maxX : localX;
							minY = minY < localY ? minY : localY;
							maxY = maxY > localY ? maxY : localY;
							minZ = minZ < localZ ? minZ : localZ;
							maxZ = maxZ > localZ ? maxZ : localZ;
						}
						
						// Check if command has an X value
						if(gcode.hasValue('X')) {
						
							// Get X value of the command
							commandX = stod(gcode.getValue('X'));
							
							// Set local X
							localX = relativeMode ? localX + commandX : commandX;
						}
						
						// Check if command has a Y value
						if(gcode.hasValue('Y')) {
						
							// Get Y value of the command
							commandY = stod(gcode.getValue('Y'));
							
							// Set local Y
							localY = relativeMode ? localY + commandY : commandY;
						}
						
						// Check if command has a X value
						if(gcode.hasValue('Z')) {
						
							// Get X value of the command
							commandZ = stod(gcode.getValue('Z'));
							
							// Set local Z
							localZ = relativeMode ? localZ + commandZ : commandZ;
						}
						
						// Check if positive extruding
						if(positiveExtrusion) {
						
							// Set minimums and maximums
							minX = minX < localX ? minX : localX;
							maxX = maxX > localX ? maxX : localX;
							minY = minY < localY ? minY : localY;
							maxY = maxY > localY ? maxY : localY;
							minZ = minZ < localZ ? minZ : localZ;
							maxZ = maxZ > localZ ? maxZ : localZ;
						}
					
					break;
					
					case 90:
					
						// Clear relative mode
						relativeMode = false;
					break;
				
					case 91:
					
						// Set relative mode
						relativeMode = true;
					break;
				}
			}
		}
		
		// Set X, Y, and Z sizes of the print
		sizeX = maxX - minX;
		sizeY = maxY - minY;
		sizeZ = maxZ - minZ;
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

uint16_t getBoundedTemperature(uint16_t temperature) {

	// Return temperature bounded by range
	return temperature > 285 ? 285 : temperature < 150 ? 150 : temperature;
}

double getDistance(const Gcode &firstPoint, const Gcode &secondPoint) {

	// Return distance between the two values
	return sqrt(pow(stod(firstPoint.getValue('X')) - stod(secondPoint.getValue('X')), 2) + pow(stod(firstPoint.getValue('Y')) - stod(secondPoint.getValue('Y')), 2));
}

bool isSharpCorner(const Gcode &point, const Gcode &refrence) {

	// Initialize variables
	double currentX = stod(point.getValue('X'));
	double currentY = stod(point.getValue('Y'));
	double previousX = stod(refrence.getValue('X'));
	double previousY = stod(refrence.getValue('Y'));
	
	// Calculate value
	double value = acos((currentX * previousX + currentY * previousY) / (pow(currentX * currentX + currentY * currentY, 2) * pow(previousX * previousX + previousY * previousY, 2)));
	
	// Return if sharp corner
	return value > 0 && value < M_PI_2;
}

Gcode createTackPoint(const Gcode &point, const Gcode &refrence) {

	// Initialize variables
	Gcode gcode;
	uint32_t time = ceil(getDistance(point, refrence));
	
	// Check if time is greater than 5
	if(time > 5) {
	
		// Set g-code to a delay command based on time
		gcode.setValue('G', "4");
		gcode.setValue('P', to_string(time));
	}
	
	// Return gcode
	return gcode;
}

bool basicPreparationPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Add intro to temp
		temp << "M106 S" << (filament == PLA ? "255" : "50") << endl;
		temp << "M17" << endl;
		temp << "G90" << endl;
		temp << "M104 S" << to_string(temperature) << endl;
		temp << "G0 Z5 F2900" << endl;
		temp << "G28" << endl;
		temp << "M18" << endl;
		temp << "M109 S" << to_string(temperature) << endl;
		temp << "G4 S10" << endl;
		temp << "M17" << endl;
		temp << "G91" << endl;
		temp << "G0 E7.5 F2000" << endl;
		temp << "G92 E0" << endl;
		temp << "G90" << endl;
		temp << "G0 F2400" << endl;
		temp << "; can extrude" << endl;
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Pase line
			gcode.parseLine(line);
			
			// Check if command controls extruder temperature or fan speed
			if(gcode.hasValue('M') && (gcode.getValue('M') == "104" || gcode.getValue('M') == "106" || gcode.getValue('M') == "107" || gcode.getValue('M') == "109"))
			
				// Get next line
				continue;
			
			// Send line to temp
			temp << gcode << endl;
		}
	
		// Add outro to temp
		temp << "G91" << endl;
		temp << "G0 E-1 F2000" << endl;
		temp << "G0 X5 Y5 F2000" << endl;
		temp << "G0 E-8 F2000" << endl;
		temp << "M104 S0" << endl;
		if(sizeZ > 60) {
			if(sizeZ < 110)
				temp << "G0 Z3 F2900" << endl;
			temp << "G90" << endl;
			temp << "G0 X90 Y84" << endl;
		}
		else {
			temp << "G0 Z3 F2900" << endl;
			temp << "G90" << endl;
			temp << "G0 X95 Y95" << endl;
		}
		temp << "M107" << endl;
		temp << "M18" << endl;
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool waveBondingPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Check if line was parsed successfully
			gcode.parseLine(line);
				
			// Send line to temp
			temp << gcode << endl;
		}
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool thermalBondingPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode, previousGcode, refrenceGcode, tackPoint;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	int layerCounter = 0, cornerCounter = 0;
	bool flag = false;
	bool relativeMode = false;
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Check if line is a layer commend
			if(line.find(";LAYER:") != string::npos) {
			
				// Check if layerCounter is 0
				if(!layerCounter) {
				
					// Send temperature command to temp
					temp << "M109 S" << to_string(getBoundedTemperature(temperature + (filament == PLA ? 10 : 15))) << endl;
					// Set flag
					flag = true;
				}
				
				// Otherwise check if layerCounter is one
				else if(layerCounter == 1)
				
					// Send temperature command to temp
					temp << "M109 S" << to_string(getBoundedTemperature(temperature + (filament == PLA ? 5 : 10))) << endl;
				// Increment layer counter
				layerCounter++;
			}
			
			// Check if line is layer zero
			if(line.find(";LAYER:0") != string::npos) {
			
				// Send temperature command to temp
				temp << "M109 S" << to_string(temperature) << endl;
				
				// Clear flag
				flag = false;
			}
			
			// Check if line was parsed successfully and it's a G command and wave bonding is not being used
			if(gcode.parseLine(line) && gcode.hasValue('G') && !useWaveBonding) {
			
				// Check what parameter is associated with the command
				switch(stoi(gcode.getValue('G'))) {
				
					case 0:
					case 1:
					
						// Check if previous command exists, the flag is set, and filament is ABS, HIPS, or PLA
						if(!previousGcode.isEmpty() && flag && (filament == ABS || filament == HIPS || filament == PLA)) {
							// Check if both counters are less than or equal to one
							if(cornerCounter <= 1 && layerCounter <= 1) {
							
								// Check if sharp corner
								if(isSharpCorner(gcode, previousGcode)) {
								
									// Check if refrence g-codes is set
									if(refrenceGcode.isEmpty()) {
									
										// Check if a tack point was created
										tackPoint = createTackPoint(gcode, previousGcode);
										if(!tackPoint.isEmpty())
										
											// Send tack point to temp
											temp << tackPoint << endl;
									}
									
									// Set refrence g-code
									refrenceGcode = gcode;
									
									// Increment corner count
									cornerCounter++;
								}
						
							}
							
							// Otherwise check if corner counter is greater than one but layer counter isn't and sharp corner
							else if(cornerCounter >= 1 && layerCounter <= 1 && isSharpCorner(gcode, refrenceGcode)) {
							
								// Check if a tack point was created
								tackPoint = createTackPoint(gcode, refrenceGcode);
								if(!tackPoint.isEmpty())
								
									// Send tack point to temp
									temp << tackPoint << endl;
								
								
								// Set refrence g-code
								refrenceGcode = gcode;
							}
						}
					break;
					
					case 90:
					
						// Clear relative mode
						relativeMode = false;
					break;
				
					case 91:
					
						// Set relative mode
						relativeMode = true;
					break;
				}
			}
			
			// Set previous g-code
			previousGcode = gcode;
			
			// Check if not using wave bonding, filament is ABS, command contains G and Z, and in absolute mode
			if(!useWaveBonding && filament == ABS && gcode.hasValue('G') && gcode.hasValue('Z') && !relativeMode)
				
				// Adjust g-code to have Z lower by 0.1
				gcode.setValue('Z', to_string(stod(gcode.getValue('Z')) - 0.1));
			
			// Send g-code to temp
			temp << gcode << endl;
		}
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool bedCompensationPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Check if line was parsed successfully
			gcode.parseLine(line);
				
			// Send line to temp
			temp << gcode << endl;
		}
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool backlashCompensationPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Check if line was parsed successfully
			gcode.parseLine(line);
				
			// Send line to temp
			temp << gcode << endl;
		}
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}

bool feedRateConversionPreprocessor() {

	// Initialzie variables
	string line;
	Gcode gcode;
	fstream processedFile(folderLocation + "/output.gcode", ios::in | ios::binary);
	fstream temp(folderLocation + "/temp", ios::out | ios::in | ios::binary | ios::app);
	
	// Check if temp file was opened successfully
	if(processedFile.good() && temp.good()) {
	
		// Go through processed file
		while(processedFile.peek() != EOF) {
	
			// Read in line
			getline(processedFile, line);
			
			// Check if line was parsed successfully
			gcode.parseLine(line);
				
			// Send line to temp
			temp << gcode << endl;
		}
		
		// Transfer contents of temp to processed file
		temp.clear();
		temp.seekg(0, ios::beg);
		processedFile.close();
		processedFile.open(folderLocation + "/output.gcode", ios::out | ios::binary | ios::trunc);
		processedFile << temp.rdbuf();
		processedFile.close();
		
		// Delete temp
		unlink((folderLocation + "/temp").c_str());
		
		// Return true
		return true;
	}
	
	// Return false
	return false;
}
