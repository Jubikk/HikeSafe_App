import React, { useRef, useEffect, useState } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  FlatList,
  ActivityIndicator,
  Alert,
  StyleSheet,
} from 'react-native';
import { BLE_CONFIG } from '../../config/constants';
import AsyncStorage from '@react-native-async-storage/async-storage';
import DeviceItem from './DeviceItem';

const ConnectionScreen = ({
  bleState,
  isConnected,
  isScanning,
  setIsScanning,
  availableDevices,
  setAvailableDevices,
  debugInfo,
  messages,
  connectToDevice,
  clearMessages,
  bleManagerRef,
  addDebugInfo,
  onSkip,
  onConnect,
}) => {
  const [localDebugInfo, setLocalDebugInfo] = useState('');
  const scanTimeoutRef = useRef(null);

  const scanForDevices = async () => {
    const debugMsg = `Initiating BLE scan. Manager ready: ${!!bleManagerRef.current}, BLE state: ${bleState}`;
    addDebugInfo(debugMsg);
    setLocalDebugInfo(prev => `${new Date().toISOString()}: ${debugInfo}\n${prev}`);
    
    if (!bleManagerRef?.current) {
      const errorMsg = 'BLE manager not ready';
      addDebugInfo(errorMsg);
      setLocalDebugInfo(prev => `${new Date().toISOString()}: ${errorMsg}\n${prev}`);
      Alert.alert('BLE Not Ready', 'Bluetooth manager is not initialized yet.');
      return;
    }

    if (bleState !== 'PoweredOn') {
      addDebugInfo(`BLE state not ready: ${bleState}`);
      Alert.alert('Bluetooth Not Ready', `Bluetooth state: ${bleState}. Please ensure Bluetooth is turned on.`);
      return;
    }

    if (isScanning) {
      addDebugInfo('Already scanning');
      return;
    }

    setIsScanning(true);
    setAvailableDevices([]);
    addDebugInfo('Starting BLE scan...');

    try {
      bleManagerRef.current.startDeviceScan(
        null,
        { allowDuplicates: false },
        (error, device) => {
          if (error) {
            addDebugInfo(`Scan error: ${error.message}`);
            console.error('Scan error:', error);
            setIsScanning(false);
            return;
          }
          if (device) {
            setAvailableDevices(prevDevices => {
              const exists = prevDevices.some(d => d.id === device.id);
              return exists ? prevDevices : [...prevDevices, device];
            });
          }
        }
      );

      // Stop scanning after timeout
      scanTimeoutRef.current = setTimeout(() => {
        stopScan();
      }, BLE_CONFIG.SCAN_TIMEOUT);

    } catch (error) {
      addDebugInfo(`Scan error: ${error.message}`);
      console.error('Scan error:', error);
      setIsScanning(false);
    }
  };

  const stopScan = () => {
    if (bleManagerRef.current) {
      bleManagerRef.current.stopDeviceScan();
    }
    if (scanTimeoutRef.current) {
      clearTimeout(scanTimeoutRef.current);
      scanTimeoutRef.current = null;
    }
    setIsScanning(false);
    addDebugInfo('BLE scan stopped');
  };

  // Handle device connection
  const handleConnect = async (device) => {
    try {
      addDebugInfo(`Attempting to connect to device: ${device.name || device.id}`);
      await connectToDevice(device);
      addDebugInfo(`Successfully connected to ${device.name || device.id}`);
      if (onConnect) {
        onConnect();
      } else {
        addDebugInfo('Warning: onConnect callback is not defined');
      }
    } catch (error) {
      const errorMsg = `Connection error: ${error.message || error}`;
      console.error(errorMsg, error);
      addDebugInfo(errorMsg);
      Alert.alert('Connection Error', 'Failed to connect to the device. Please try again.');
    }
  };

  // Auto-start scanning when component mounts
  useEffect(() => {
    const initScan = async () => {
      addDebugInfo('ConnectionScreen mounted, initializing...');
      await scanForDevices();
    };
    
    initScan();
    
    // Clean up on unmount
    return () => {
      stopScan();
      addDebugInfo('ConnectionScreen unmounted');
    };
  }, []);

  const handleSkip = async () => {
    try {
      await AsyncStorage.setItem('bluetoothSkipped', 'true');
      if (onSkip) {
        onSkip();
      } else {
        addDebugInfo('Warning: onSkip callback is not defined');
      }
    } catch (error) {
      console.error('Error saving skip status:', error);
      addDebugInfo(`Error saving skip status: ${error.message}`);
    }
  };

  return (
    <View style={styles.connectionContainer}>
      <View style={styles.header}>
        <Text style={styles.title}>Connect to Device</Text>
        <TouchableOpacity 
          style={styles.refreshButton} 
          onPress={scanForDevices}
          disabled={isScanning}
        >
          <Text style={styles.refreshText}>⟳</Text>
        </TouchableOpacity>
      </View>
      <Text style={styles.status}>
        Status: {bleState || 'Unknown'}
        {isConnected ? ' (Connected)' : ' (Not Connected)'}
      </Text>
      
      <TouchableOpacity
        style={[styles.button, isScanning && styles.buttonDisabled]}
        onPress={isScanning ? stopScan : scanForDevices}
        disabled={isScanning}
      >
        {isScanning ? (
          <ActivityIndicator color="#fff" />
        ) : (
          <Text style={styles.buttonText}>Scan for Devices</Text>
        )}
      </TouchableOpacity>

      {availableDevices.length > 0 ? (
        <FlatList
          data={availableDevices}
          renderItem={({item}) => (
            <DeviceItem
              device={item}
              onPress={() => handleConnect(item)}
            />
          )}
          keyExtractor={(item) => item.id}
          contentContainerStyle={styles.deviceList}
        />
      ) : (
        <View style={styles.noDevicesContainer}>
          <Text style={styles.noDevicesText}>No devices found</Text>
          <Text style={styles.hintText}>
            Make sure your device is powered on and in range
          </Text>
        </View>
      )}

      <TouchableOpacity
        style={[styles.skipButton, styles.button]}
        onPress={handleSkip}
      >
        <Text style={styles.buttonText}>Skip for Now</Text>
      </TouchableOpacity>

      <View style={styles.debugContainer}>
        <Text style={styles.debugTitle}>Debug Info:</Text>
        <Text style={styles.debugText}>
          {localDebugInfo || 'No debug information available'}
        </Text>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  connectionContainer: {
    flex: 1,
    backgroundColor: '#f5f5f5',
    padding: 20,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  refreshButton: {
    padding: 8,
  },
  refreshText: {
    fontSize: 20,
    color: '#2c3e50',
  },
  title: {
    fontSize: 20,
    fontWeight: 'bold',
    color: '#333',
  },
  status: {
    fontSize: 16,
    color: '#666',
    marginBottom: 20,
  },
  button: {
    backgroundColor: '#2c3e50',
    padding: 15,
    borderRadius: 5,
    marginVertical: 10,
    alignItems: 'center',
  },
  buttonDisabled: {
    backgroundColor: '#ccc',
  },
  buttonText: {
    color: '#fff',
    fontWeight: '600',
  },
  deviceList: {
    paddingBottom: 20,
  },
  noDevicesContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  noDevicesText: {
    fontSize: 16,
    color: '#666',
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
    color: '#999',
    textAlign: 'center',
  },
  skipButton: {
    backgroundColor: '#95a5a6',
    marginTop: 20,
    borderRadius: 8,
    padding: 15,
    alignItems: 'center',
  },
  debugContainer: {
    marginTop: 'auto',
    backgroundColor: '#f5f5f5',
    padding: 10,
    borderRadius: 8,
  },
  debugTitle: {
    fontWeight: 'bold',
    marginBottom: 4,
    color: '#666',
  },
  debugText: {
    fontFamily: 'monospace',
    fontSize: 12,
    color: '#666',
  },
});

export default ConnectionScreen;
