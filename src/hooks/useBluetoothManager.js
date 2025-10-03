import { useState, useEffect, useCallback } from 'react';
import { Alert } from 'react-native';
import { BLE_CONFIG } from '../config/constants';

export const useBluetoothManager = (bleManagerRef, addDebugInfo) => {
  const [device, setDevice] = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState(null);
  const [isInitialized, setIsInitialized] = useState(false);

  // Initialize Bluetooth manager once
  useEffect(() => {
    const init = async () => {
      try {
        if (!isInitialized) {
          addDebugInfo('Initializing Bluetooth Manager...');
          setIsInitialized(true);
        }
      } catch (err) {
        setError(`Bluetooth initialization failed: ${err.message}`);
        Alert.alert('Bluetooth Error', 'Failed to initialize Bluetooth. Please restart the app.');
      }
    };

    init();
  }, [addDebugInfo, isInitialized]);

  // Connect to a device
  const connectToDevice = useCallback(async (deviceId) => {
    if (!deviceId || typeof deviceId !== 'string') {
      throw new Error('Invalid device ID');
    }

    try {
      addDebugInfo(`Connecting to: ${deviceId.substring(0, 8)}...`);
      
      // Disconnect existing device first
      if (device && isConnected) {
        try {
          await device.cancelConnection();
        } catch (e) {
          console.warn('Error disconnecting existing device:', e);
        }
      }

      const connectedDevice = await bleManagerRef.current.connectToDevice(deviceId);
      addDebugInfo(`Connected to: ${connectedDevice.name || 'Unknown'}`);

      await connectedDevice.discoverAllServicesAndCharacteristics();
      addDebugInfo('Services discovered');

      setDevice(connectedDevice);
      setIsConnected(true);
      setError(null);
      
      return connectedDevice;
    } catch (err) {
      const errorMsg = `Connection failed: ${err.message}`;
      addDebugInfo(errorMsg);
      setError(errorMsg);
      throw err;
    }
  }, [device, isConnected, bleManagerRef, addDebugInfo]);

  // Disconnect from current device
  const disconnect = useCallback(async () => {
    if (device) {
      try {
        await device.cancelConnection();
        addDebugInfo('Disconnected from device');
      } catch (err) {
        console.warn('Error disconnecting:', err);
      } finally {
        setDevice(null);
        setIsConnected(false);
      }
    }
  }, [device, addDebugInfo]);

  // Clean up on unmount
  useEffect(() => {
    return () => {
      if (device && isConnected) {
        disconnect();
      }
    };
  }, [device, isConnected, disconnect]);

  return {
    device,
    isConnected,
    error,
    connectToDevice,
    disconnect,
  };
};
