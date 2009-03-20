// For conditions of distribution and use, see copyright notice in license.txt
#include "StableHeaders.h"

#include "NetTestLogicModule.h"
#include <Poco/ClassLibrary.h>
#include "Foundation.h"

#include "RexProtocolMsgIDs.h"

/// Login credentials. 
///\todo Use real ones, not hardcoded.
/*const char serverAddress[] = "192.168.1.144";
const int port = 9000;
const char firstName[] = "jj";
const char lastName[] = "jj";
const char password[] = "jj";*/

namespace NetTest
{
    NetTestLogicModule::NetTestLogicModule() 
    : ModuleInterface_Impl("NetTest"),
    bRunning_(false),
    bLogoutSent_(false),
    loginWindow(0),
    netTestWindow(0)
    {
        objectList_.clear();
        avatarList_.clear();
    }

    NetTestLogicModule::~NetTestLogicModule()
    {
    	for(ObjectList_t::iterator iter = objectList_.begin(); iter != objectList_.end(); ++iter)
		    SAFE_DELETE(iter->second);

    	for(ObjectList_t::iterator iter = avatarList_.begin(); iter != avatarList_.end(); ++iter)
		    SAFE_DELETE(iter->second);
    }

    // virtual
    void NetTestLogicModule::Load()
    {
        LogInfo("Module " + Name() + " loaded.");
    }

    // virtual
    void NetTestLogicModule::Unload()
    {
        LogInfo("Module " + Name() + " unloaded.");
    }

    // virtual
    void NetTestLogicModule::Initialize(Foundation::Framework *framework)
    {
        assert(framework != NULL);
        framework_ = framework;
		
		using namespace OpenSimProtocol;
		///\todo weak_pointerize
		netInterface_ = dynamic_cast<OpenSimProtocolModule *>(framework->GetModuleManager()->GetModule("Network"));
		if (!netInterface_)
		{
			LogError("Getting network interface did not succeed.");
			//framework_->GetModuleManager()->UninitializeModule(this);
			return;
		}
		
		netInterface_->AddListener(this);
        
        LogInfo("Module " + Name() + " initialized.");        
        
        InitLoginWindow();
        InitNetTestWindow();

        if(!loginWindow || !netTestWindow)
        {
            LogError("Could not initialize UI.");
            return;
        }
        
        loginWindow->show();
    }

    // virtual 
    void NetTestLogicModule::Uninitialize(Foundation::Framework *framework)
    {
		assert(framework_ != NULL);
		framework_ = NULL;
        
        netInterface_->RemoveListener(this);
        
        SAFE_DELETE(netTestWindow)
        SAFE_DELETE(loginWindow)
        
        LogInfo("Module " + Name() + " uninitialized.");
    }

    // virtual
    void NetTestLogicModule::Update()
    {

    }
    
    //virtual 
    void NetTestLogicModule::OnNetworkMessageReceived(NetMsgID msgID, NetInMessage *msg)
    {
    	switch(msgID)
		{
		case RexNetMsgRegionHandshake:
			{
				LogInfo("\"RegionHandshake\" received, " + Core::ToString(msg->GetDataSize()) + " bytes.");
				msg->SkipToNextVariable(); // RegionFlags U32
				msg->SkipToNextVariable(); // SimAccess U8
				size_t bytesRead = 0;
				simName_ = (const char *)msg->ReadBuffer(&bytesRead);
				
				LogInfo("Joined to the sim \"" + simName_ + "\".");
				
				std::string title = "Logged in to ";
				title.append(simName_);
                netTestWindow->set_title(title);
    			break;
			}
		case RexNetMsgObjectUpdate:
			{
				LogInfo("\"ObjectUpdate\" received, " + Core::ToString(msg->GetDataSize()) + " bytes.");
				
				Object *obj = new Object;
				msg->SkipToNextVariable();		// RegionHandle U64
				msg->SkipToNextVariable();		// TimeDilation U16
				obj->localID = msg->ReadU32(); 
				msg->SkipToNextVariable();		// State U8
				obj->fullID = msg->ReadUUID();
				msg->SkipToNextVariable();		// CRC U32
				uint8_t PCode = msg->ReadU8();
				
				ObjectList_t::iterator it;
    			if (PCode == 0x09)
				{
				    // If prim, PCode is 0x09.
				    it = std::find_if(objectList_.begin(), objectList_.end(), IDMatchPred(obj->fullID));
				    if (it != objectList_.end())
				    {
				        // Do not add duplicates.
				        SAFE_DELETE(obj);
				        return;
				    }
				    else
				        objectList_.push_back(std::make_pair(obj->fullID, obj));
                }
				else if (PCode == 0x2f)
				{
				    // If avatar, PCode is 0x2f.
				    it = std::find_if(avatarList_.begin(), avatarList_.end(), IDMatchPred(obj->fullID));
				    if (it != avatarList_.end())
				    {
				        // Do not add duplicates.
				        SAFE_DELETE(obj);				    
				        return;
				    }
				    
				    // Read the name of the avatar.
				    msg->SkipToFirstVariableByName("NameValue");
					size_t bytesRead = 0;
					std::string name = (const char *)msg->ReadBuffer(&bytesRead);
    					
					// Parse the name.
					std::string first = "FirstName STRING RW SV ";
					std::string last = "LastName STRING RW SV ";
					size_t pos;

					pos = name.find(first);
					name.replace(pos, strlen(first.c_str()), "");
					pos = name.find(last);
					name.replace(pos, strlen(last.c_str()), "");
					pos = name.find("\n");
					name.replace(pos, 1, " ");
					obj->name = name;
					avatarList_.push_back(std::make_pair(obj->fullID, obj));
					
					LogInfo("Avatar \"" + name + "\" joined the sim");
                }
                else
                    //We're not interested in any other objects at the moment.
                    SAFE_DELETE(obj);
                    
				break;
			}
		case RexNetMsgChatFromSimulator:
		    {
		        std::stringstream ss;
		        size_t bytes_read;

		        std::string name = (const char *)msg->ReadBuffer(&bytes_read);
		        msg->SkipToFirstVariableByName("Message");

		        std::string message = (const char *)msg->ReadBuffer(&bytes_read);
		        ss << name << ": " << message << std::endl;

    	        WriteToChatWindow(ss.str());
		        break;
		    }
		case RexNetMsgLogoutReply:
			{
			    LogInfo("\"LogoutReply\" received, " + Core::ToString(msg->GetDataSize()) + " bytes.");
				RexUUID aID = msg->ReadUUID();
				RexUUID sID = msg->ReadUUID();
	
				// Logout if the id's match.
				if (aID == myInfo_.agentID && sID == myInfo_.sessionID)
				{
					LogInfo("\"LogoutReply\" received with matching IDs. Loggin out.");
                    bRunning_ = false;
                    bLogoutSent_ = false;
                    netInterface_->DisconnectFromRexServer();
                    SAFE_DELETE(netTestWindow);
				}
				break;
			}
		default:
			netInterface_->DumpNetworkMessage(msgID, msg);
			break;
		}        
    }
    
    void NetTestLogicModule::OnClickConnect()
    {
        if(bRunning_)
        {
            LogError("You are already connected to a server!");
            return;
        }
        
        // Initialize UI widgets.
        Gtk::Entry *entryUsername;
        Gtk::Entry *entryPassword;
        Gtk::Entry *entryServer;
        loginControls->get_widget("entry_username", entryUsername);
        loginControls->get_widget("entry_password", entryPassword);
        loginControls->get_widget("entry_server", entryServer);

        size_t pos;
        bool rex_login = false; ///\todo Implement rex-login.
        std::string username, first_name, last_name, password, server_address;
        int port;
        
        // Get username.
        if (!rex_login)
        {
            username = entryUsername->get_text();
            pos = username.find(" ");
            if(pos == std::string::npos)
            {
                LogError("Invalid username.");
                return;
            }
            first_name = username.substr(0, pos);
            last_name = username.substr(pos + 1);
        }
        else
            username = entryUsername->get_text();
        
        // Get server address and port.
        std::string server = entryServer->get_text();
        pos = server.find(":");
        if(pos == std::string::npos)
        {
            LogError("Invalid syntax for server address and port. Use \"server:port\"");
            return;
        }
        
        server_address = server.substr(0, pos);
        try
        {
            port = boost::lexical_cast<int>(server.substr(pos + 1));
        } catch(std::exception)
        {
            LogError("Invalid port number, only numbers are allowed.");
            return;
        }
        
        // Get password.
        password = entryPassword->get_text();
        
        bool success = netInterface_->ConnectToRexServer(first_name.c_str(), last_name.c_str(),
            password.c_str(), server_address.c_str(), port, &myInfo_);
        if(success)
        {   
            bRunning_ = true;
            SendUseCircuitCodePacket();
            SendCompleteAgentMovementPacket();
            
            if (!netTestWindow)
                InitNetTestWindow();
            
            netTestWindow->show();
            
            LogInfo("Connected to server " + server_address + ".");
        }
        else
            LogError("Connecting to server " + server_address + " failed.");
    }
    
    void NetTestLogicModule::OnClickLogout()
    {
        if (bRunning_ && !bLogoutSent_)
        {
            SendLogoutRequestPacket();
            bLogoutSent_ = true;
        }
        ///\todo Handle server timeouts.
    }
    
    void NetTestLogicModule::OnClickQuit()
    {
        if (bRunning_ && !bLogoutSent_)
        {   
            SendLogoutRequestPacket();
            bLogoutSent_ = true;        
        }
        else
        {
            framework_->Exit();
            assert(framework_->IsExiting());        
        }
    }
    
    void NetTestLogicModule::OnClickChat()
    {
        Gtk::Entry *entryChat = 0;
        netTestControls->get_widget("entry_chat", entryChat);
        Glib::ustring text  = entryChat->get_text();
        SendChatFromViewerPacket(text.c_str());
        entryChat->set_text("");
    }
    
    void NetTestLogicModule::WriteToChatWindow(const std::string &message)
    {   
        // Get the widget controls.
        Gtk::ScrolledWindow *scrolledwindowChat = 0;
		netTestControls->get_widget("scrolledwindow_chat", scrolledwindowChat);
		Gtk::TextView *textviewChat = 0;
		netTestControls->get_widget("textview_chat", textviewChat);
		scrolledwindowChat->set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS);
		
		// Create text buffer and write data to it.
		Glib::RefPtr<Gtk::TextBuffer> text_buffer = textviewChat->get_buffer();
        Gtk::TextBuffer::iterator iter = text_buffer->get_iter_at_offset(0);
		text_buffer->insert(iter, message);
        textviewChat->set_buffer(text_buffer);
    }
    
	void NetTestLogicModule::SendUseCircuitCodePacket()
	{
		NetOutMessage *m = netInterface_->StartMessageBuilding(RexNetMsgUseCircuitCode);
		assert(m);
		m->AddU32(myInfo_.circuitCode);
		m->AddUUID(myInfo_.sessionID);
		m->AddUUID(myInfo_.agentID);
		netInterface_->FinishMessageBuilding(m);
	}

    void NetTestLogicModule::SendCompleteAgentMovementPacket()
    {
        NetOutMessage *m = netInterface_->StartMessageBuilding(RexNetMsgCompleteAgentMovement);
	    assert(m);
	    m->AddUUID(myInfo_.agentID);
	    m->AddUUID(myInfo_.sessionID);
	    m->AddU32(myInfo_.circuitCode);
	    netInterface_->FinishMessageBuilding(m);
    }

	void NetTestLogicModule::SendChatFromViewerPacket(const char *text)
	{
		NetOutMessage *m = netInterface_->StartMessageBuilding(RexNetMsgChatFromViewer);
		assert(m);
		m->AddUUID(myInfo_.agentID);
		m->AddUUID(myInfo_.sessionID);
		m->AddBuffer(strlen(text), (uint8_t*)text);
		m->AddU8(1);
		m->AddS32(0);
		netInterface_->FinishMessageBuilding(m);
	}
    
	void NetTestLogicModule::SendLogoutRequestPacket()
	{
		NetOutMessage *m = netInterface_->StartMessageBuilding(RexNetMsgLogoutRequest);
		assert(m);
    	m->AddUUID(myInfo_.agentID);
		m->AddUUID(myInfo_.sessionID);
	    netInterface_->FinishMessageBuilding(m);
	}

    void NetTestLogicModule::InitLoginWindow()
    {
        // Create the login window from glade (xml) file.
        loginControls = Gnome::Glade::Xml::create("data/loginWindow.glade");
        if (!loginControls)
            return;
        
        loginControls->get_widget("dialog_login", loginWindow);
        loginWindow->set_title("Login");
       
        // Bind callbacks.
        loginControls->connect_clicked("button_connect", sigc::mem_fun(*this, &NetTestLogicModule::OnClickConnect));
        loginControls->connect_clicked("button_logout", sigc::mem_fun(*this, &NetTestLogicModule::OnClickLogout));
        loginControls->connect_clicked("button_quit", sigc::mem_fun(*this, &NetTestLogicModule::OnClickQuit));
    }
    
    void NetTestLogicModule::InitNetTestWindow()
    {
        // Create the NetTest UI window from glade (xml) file.
        netTestControls = Gnome::Glade::Xml::create("data/NetTestWindow.glade");
        if (!netTestControls)
            return;
        
        netTestControls->get_widget("window_nettest", netTestWindow);
        netTestWindow->set_title("NetTest");
        
        // Initialize UI widgets.
        Gtk::TextView *textviewLog = 0;
        netTestControls->get_widget("textview_log", textviewLog);
        
        // Bind callbacks.
        netTestControls->connect_clicked("button_chat", sigc::mem_fun(*this, &NetTestLogicModule::OnClickChat));
    }
}

using namespace NetTest;

POCO_BEGIN_MANIFEST(Foundation::ModuleInterface)
   POCO_EXPORT_CLASS(NetTestLogicModule)
POCO_END_MANIFEST
