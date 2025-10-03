// src/providers/AppProvider.js - Context Provider
import React, { createContext, useContext, useEffect } from 'react';
import { useBLE } from '../hooks/useBLE';
import { useMessages } from '../hooks/useMessages';
import { useDebug } from '../hooks/useDebug';
import { useDeviceConnection } from '../hooks/useDeviceConnection';
import { useAppFlow } from '../hooks/useAppFlow';
import { useBluetoothManager } from '../hooks/useBluetoothManager';
import { initDB } from '../database/database';

const AppContext = createContext();

export const useAppContext = () => {
  const context = useContext(AppContext);
  if (!context) {
    throw new Error('useAppContext must be used within AppProvider');
  }
  return context;
};

export default function AppProvider({ children }) {
  // Initialize database once at startup
  useEffect(() => {
    initDB();
  }, []);

  // Custom hooks
  const { debugInfo, addDebugInfo } = useDebug();
  const { bleManagerRef, bleState, permissionsGranted } = useBLE(addDebugInfo);
  
  // Initialize Bluetooth manager
  const {
    device,
    isConnected,
    connectToDevice,
    disconnect: disconnectDevice,
    error: bluetoothError
  } = useBluetoothManager(bleManagerRef, addDebugInfo);
  
  const { messages, setMessages, addMessage, clearMessages } = useMessages(addDebugInfo);
  
  // Initialize device connection for messaging
  const { meshStatus, sendMessage } = useDeviceConnection(
    device, 
    isConnected, 
    addDebugInfo, 
    addMessage
  );
  
  // Clean up on unmount
  useEffect(() => {
    return () => {
      if (isConnected && device) {
        disconnectDevice();
      }
    };
  }, [isConnected, device, disconnectDevice]);
  
  const appFlowState = useAppFlow();
  const { 
    showBluetoothConnection, 
    skipBluetoothConnection, 
    completeBluetoothConnection,
    ...restAppFlowState 
  } = appFlowState;

  return (
    <AppContext.Provider value={{
      // State
      debugInfo,
      messages,
      bleState,
      permissionsGranted,
      isBluetoothReady: true, // Always true as we handle initialization internally
      isConnected,
      device,
      meshStatus,
      bluetoothError,
      showBluetoothConnection,
      // Methods
      skipBluetoothConnection,
      addDebugInfo,
      addMessage,
      clearMessages,
      connectToDevice,
      disconnect: disconnectDevice,
      sendMessage,
      completeBluetoothConnection,
      // App Flow
      ...restAppFlowState,
    }}>
      {children}
    </AppContext.Provider>
  );
}