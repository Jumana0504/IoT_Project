import 'package:flutter/material.dart';
import 'package:firebase_auth/firebase_auth.dart';
import 'package:http/http.dart' as http;
import 'dart:convert';

class ProvisioningScreen extends StatefulWidget {
  const ProvisioningScreen({super.key});

  @override
  State<ProvisioningScreen> createState() => _ProvisioningScreenState();
}

class _ProvisioningScreenState extends State<ProvisioningScreen> {
  final _ssidController = TextEditingController();
  final _passController = TextEditingController();
  bool _isLoading = false;
  String? _message;

  Future<void> _provisionDevice() async {
    final ssid = _ssidController.text.trim();
    final pass = _passController.text.trim();
    final user = FirebaseAuth.instance.currentUser;

    if (ssid.isEmpty || pass.isEmpty || user == null) {
      setState(() => _message = 'All fields are required.');
      return;
    }

    setState(() {
      _isLoading = true;
      _message = null;
    });

    final body = {
      'ssid': ssid,
      'password': pass,
      'userId': user.uid,
    };

    try {
      final response = await http.post(
        Uri.parse('http://192.168.4.1/setup'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode(body),
      );

      setState(() {
        _message = response.statusCode == 200
            ? '✅ Provisioning sent successfully!'
            : '❌ Failed: ${response.body}';
      });
    } catch (e) {
      setState(() => _message = '❌ Error: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Provision Smart AC')),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          children: [
            const Text(
              'Make sure you are connected to the AC’s Wi-Fi (SmartAC-Setup-XXXX)',
              style: TextStyle(fontSize: 14, fontStyle: FontStyle.italic),
            ),
            const SizedBox(height: 20),
            TextField(
              controller: _ssidController,
              decoration: const InputDecoration(labelText: 'Home Wi-Fi SSID'),
            ),
            TextField(
              controller: _passController,
              decoration: const InputDecoration(labelText: 'Wi-Fi Password'),
              obscureText: true,
            ),
            const SizedBox(height: 20),
            ElevatedButton(
              onPressed: _isLoading ? null : _provisionDevice,
              child: _isLoading
                  ? const CircularProgressIndicator(color: Colors.white)
                  : const Text('Send to Device'),
            ),
            if (_message != null) ...[
              const SizedBox(height: 20),
              Text(_message!, style: TextStyle(color: _message!.startsWith('✅') ? Colors.green : Colors.red)),
            ],
          ],
        ),
      ),
    );
  }
}