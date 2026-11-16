"""Security regression tests for the CYD LAN server (stdlib unittest only)."""

import os
import unittest
from unittest.mock import patch

from server import app, _messages


class AccessControlTests(unittest.TestCase):
    def setUp(self):
        self.client = app.test_client()
        _messages.clear()

    def test_tokenless_loopback_access_is_allowed(self):
        with patch.dict(os.environ, {}, clear=False):
            os.environ.pop("CYD_API_TOKEN", None)
            response = self.client.get(
                "/health", environ_base={"REMOTE_ADDR": "127.0.0.1"}
            )
        self.assertEqual(response.status_code, 200)

    def test_tokenless_remote_access_is_rejected(self):
        with patch.dict(os.environ, {}, clear=False):
            os.environ.pop("CYD_API_TOKEN", None)
            response = self.client.get(
                "/messages", environ_base={"REMOTE_ADDR": "192.168.1.20"}
            )
        self.assertEqual(response.status_code, 503)

    def test_configured_token_is_required_for_every_endpoint(self):
        with patch.dict(os.environ, {"CYD_API_TOKEN": "test-secret"}):
            missing = self.client.get("/health")
            wrong = self.client.get(
                "/health", headers={"Authorization": "Bearer wrong"}
            )
            correct = self.client.get(
                "/health", headers={"Authorization": "Bearer test-secret"}
            )
        self.assertEqual(missing.status_code, 401)
        self.assertEqual(wrong.status_code, 401)
        self.assertEqual(correct.status_code, 200)

    def test_authenticated_message_and_clear(self):
        headers = {"Authorization": "Bearer test-secret"}
        with patch.dict(os.environ, {"CYD_API_TOKEN": "test-secret"}):
            created = self.client.post(
                "/message",
                json={"message": "hello", "source": "test"},
                headers=headers,
            )
            listed = self.client.get("/messages", headers=headers)
            cleared = self.client.post("/messages/clear", headers=headers)
        self.assertEqual(created.status_code, 201)
        self.assertEqual(listed.get_json()["count"], 1)
        self.assertEqual(cleared.get_json()["cleared"], 1)


    def test_oversized_request_body_is_rejected(self):
        with patch.dict(os.environ, {"CYD_API_TOKEN": "test-secret"}):
            response = self.client.post(
                "/message",
                data="x" * 5000,
                content_type="application/json",
                headers={"Authorization": "Bearer test-secret"},
            )
        self.assertEqual(response.status_code, 413)


if __name__ == "__main__":
    unittest.main()
