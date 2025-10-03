import { useState, useEffect, useCallback, useRef } from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { BLE_CONFIG, STORAGE_KEYS, MESSAGE_TYPES } from '../config/constants';

export const useDeviceConnection = (device, isConnected, addDebugInfo, addMessage) => {
  const [meshStatus, setMeshStatus] = useState(null);
  const notificationSubscriptions = useRef([]);
  const isMounted = useRef(true);

  const setupNotifications = useCallback(async () => {
    if (!device || !isConnected) return;
    
    try {
      addDebugInfo('Setting up notifications...');
      
      await device.monitorCharacteristicForService(
        BLE_CONFIG.SERVICE_UUID,
        BLE_CONFIG.MESH_CHAR_UUID,
        (error, characteristic) => {
          if (error) {
            addDebugInfo(`Notification error: ${error.message}`);
            return;
          }

          if (characteristic?.value) {
            try {
              let jsonString;
              
              try {
                jsonString = atob(characteristic.value);
              } catch (e) {
                try {
                  const bytes = characteristic.value.match(/.{1,2}/g)?.map(byte => parseInt(byte, 16));
                  if (bytes && bytes.every(b => !isNaN(b) && b >= 0 && b <= 255)) {
                    jsonString = String.fromCharCode(...bytes);
                  } else {
                    jsonString = characteristic.value;
                  }
                } catch (e2) {
                  jsonString = characteristic.value;
                }
              }

              const meshData = JSON.parse(jsonString);
              addDebugInfo(`Mesh update: ${meshData.nodeCount || 0} nodes`);
              
              setMeshStatus(meshData);

              if (meshData.recentMessages && Array.isArray(meshData.recentMessages)) {
                meshData.recentMessages.forEach(msg => {
                  if (msg && msg.sender && msg.content && msg.sender !== meshData.nodeId) {
                    const messageExists = addMessage.some(existingMsg => 
                      existingMsg.text === msg.content && 
                      existingMsg.sender === msg.sender &&
                      msg.timestamp && existingMsg.timestamp &&
                      Math.abs(new Date(existingMsg.timestamp).getTime() - new Date(msg.timestamp).getTime()) < 5000
                    );
                    
                    if (!messageExists) {
                      addMessage({
                        type: MESSAGE_TYPES.RECEIVED,
                        text: msg.content,
                        sender: msg.sender,
                        rssi: msg.rssi,
                      });
                    }
                  }
                });
              }

            } catch (parseError) {
              addDebugInfo(`Parse error: ${parseError.message}`);
            }
          }
        }
      );

      addDebugInfo('Notifications setup complete');
    } catch (error) {
      addDebugInfo(`Notification setup failed: ${error.message}`);
    }
  }, [device, isConnected, addDebugInfo, addMessage]);

  const sendMessage = useCallback(async (message) => {
    if (!device || !isConnected) {
      addDebugInfo('Cannot send message: Not connected to device');
      return false;
    }
    
    try {
      await device.writeCharacteristicWithoutResponseForService(
        BLE_CONFIG.SERVICE_UUID,
        BLE_CONFIG.MESSAGE_CHAR_UUID,
        message
      );
      return true;
    } catch (error) {
      addDebugInfo(`Error sending message: ${error.message}`);
      return false;
    }
  }, [device, isConnected, addDebugInfo]);

  // Setup notifications when device connects
  useEffect(() => {
    isMounted.current = true;
    
    const setup = async () => {
      if (!isConnected || !device) return;
      
      try {
        // Clear any existing subscriptions
        notificationSubscriptions.current.forEach(sub => sub?.remove?.());
        notificationSubscriptions.current = [];
        
        // Setup mesh status notifications
        const meshSubscription = await device.monitorCharacteristicForService(
          BLE_CONFIG.SERVICE_UUID,
          BLE_CONFIG.MESH_CHAR_UUID,
          (error, characteristic) => {
            if (error) {
              addDebugInfo(`Notification error: ${error.message}`);
              return;
            }
            handleCharacteristicValue(characteristic);
          }
        );
        
        notificationSubscriptions.current.push(meshSubscription);
        addDebugInfo('Notifications setup complete');
        
      } catch (error) {
        if (isMounted.current) {
          addDebugInfo(`Notification setup error: ${error.message}`);
        }
      }
    };

    setup();

    return () => {
      isMounted.current = false;
      // Cleanup subscriptions
      notificationSubscriptions.current.forEach(sub => sub?.remove?.());
      notificationSubscriptions.current = [];
    };
  }, [isConnected, device, addDebugInfo]);
  
  const handleCharacteristicValue = (characteristic) => {
    if (!characteristic?.value) return;
    
    try {
      let jsonString;
      try {
        jsonString = atob(characteristic.value);
      } catch (e) {
        try {
          const bytes = characteristic.value.match(/.{1,2}/g)?.map(byte => parseInt(byte, 16));
          if (bytes && bytes.every(b => !isNaN(b) && b >= 0 && b <= 255)) {
            jsonString = String.fromCharCode(...bytes);
          } else {
            jsonString = characteristic.value;
          }
        } catch (e2) {
          jsonString = characteristic.value;
        }
      }

      const meshData = JSON.parse(jsonString);
      if (isMounted.current) {
        setMeshStatus(meshData);
        processIncomingMessages(meshData);
      }
    } catch (error) {
      if (isMounted.current) {
        addDebugInfo(`Error processing message: ${error.message}`);
      }
    }
  };
  
  const processIncomingMessages = (meshData) => {
    if (!meshData.recentMessages || !Array.isArray(meshData.recentMessages)) return;
    
    meshData.recentMessages.forEach(msg => {
      if (msg?.sender && msg?.content && msg.sender !== meshData.nodeId) {
        const messageExists = addMessage.some(existingMsg => 
          existingMsg.text === msg.content && 
          existingMsg.sender === msg.sender &&
          msg.timestamp && existingMsg.timestamp &&
          Math.abs(new Date(existingMsg.timestamp).getTime() - new Date(msg.timestamp).getTime()) < 5000
        );
        
        if (!messageExists) {
          addMessage({
            type: MESSAGE_TYPES.RECEIVED,
            text: msg.content,
            sender: msg.sender,
            rssi: msg.rssi,
            timestamp: msg.timestamp || new Date().toISOString()
          });
        }
      }
    });
  };

  return {
    meshStatus,
    sendMessage,
  };
};