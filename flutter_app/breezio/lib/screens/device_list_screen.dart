import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:firebase_core/firebase_core.dart';

class DeviceListScreen extends StatefulWidget {
  const DeviceListScreen({super.key});

  @override
  State<DeviceListScreen> createState() => _DeviceListScreenState();
}

class _DeviceListScreenState extends State<DeviceListScreen> {
  final _db = FirebaseDatabase.instanceFor(
  app: Firebase.app(),
  databaseURL: 'https://smart-ac-7c7a0-default-rtdb.europe-west1.firebasedatabase.app',
).ref("devices");
  final _uid = FirebaseAuth.instance.currentUser!.uid;
  Map<String, String> _devices = {};
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _loadDevices();
  }

  Future<void> _loadDevices() async {
    final snapshot = await _db.get();
    final data = snapshot.value as Map?;

    if (data == null) {
      setState(() {
        _loading = false;
        _devices = {};
      });
      return;
    }

    final Map<String, String> result = {};
    data.forEach((deviceId, deviceData) {
      final authUsers = (deviceData as Map)['authorizedUsers'] as Map?;
      final role = authUsers?[_uid];
      if (role != null) {
        result[deviceId] = role;
      }
    });

    setState(() {
      _devices = result;
      _loading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Your Smart AC Devices'),
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : _devices.isEmpty
              ? const Center(child: Text('No devices available for your user.'))
              : ListView.builder(
                  itemCount: _devices.length,
                  itemBuilder: (context, index) {
                    final deviceId = _devices.keys.elementAt(index);
                    final role = _devices[deviceId];

                    return ListTile(
                      leading: const Icon(Icons.devices),
                      title: Text('Device $deviceId'),
                      subtitle: Text('Role: $role'),
                      trailing: const Icon(Icons.arrow_forward_ios),
                      onTap: () {
                        Navigator.pushNamed(context, '/control', arguments: {
                          'deviceId': deviceId,
                          'role': role,
                        });
                      },
                    );
                  },
                ),
    );
  }
}