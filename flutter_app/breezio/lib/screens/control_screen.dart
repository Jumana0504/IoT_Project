import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:firebase_core/firebase_core.dart';


class ControlScreen extends StatelessWidget {
  const ControlScreen({super.key});

  void _sendCommand(BuildContext context, String deviceId, String action) async {
    final uid = FirebaseAuth.instance.currentUser?.uid;
    if (uid == null) return;

    final timestamp = DateTime.now().millisecondsSinceEpoch;
    final command = {
      'action': action,
      'user': uid,
      'timestamp': timestamp,
    };

    final dbRef = FirebaseDatabase.instanceFor(
      app: Firebase.app(),
      databaseURL: 'https://smart-ac-7c7a0-default-rtdb.europe-west1.firebasedatabase.app',
    ).ref('devices/$deviceId/command');

    try {
      await dbRef.set(command);
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('✅ Command "$action" sent.'),
      ));
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text('❌ Failed to send command: $e'),
      ));
    }
  }

  @override
  Widget build(BuildContext context) {
    final args = ModalRoute.of(context)!.settings.arguments as Map;
    final String deviceId = args['deviceId'];
    final String role = args['role'];

    return Scaffold(
      appBar: AppBar(
        title: Text('Control Device $deviceId'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            Text('Your Role: $role', style: const TextStyle(fontSize: 16)),
            const SizedBox(height: 30),
            ElevatedButton(
              onPressed: () => _sendCommand(context, deviceId, 'power_on'),
              child: const Text('Power ON'),
            ),
            const SizedBox(height: 10),
            if (role == 'admin')
              ElevatedButton(
                onPressed: () => _sendCommand(context, deviceId, 'power_off'),
                child: const Text('Power OFF'),
              ),
            const SizedBox(height: 10),
            ElevatedButton(
              onPressed: () => _sendCommand(context, deviceId, 'temp_up'),
              child: const Text('Temperature ↑'),
            ),
            const SizedBox(height: 10),
            ElevatedButton(
              onPressed: () => _sendCommand(context, deviceId, 'temp_down'),
              child: const Text('Temperature ↓'),
            ),
          ],
        ),
      ),
    );
  }
}