import requests

# The base address pointing to your Flask container service
BASE_URL = "http://joystick:5000"

# ----------------------------------------------------
# Private Helper Utility
# ----------------------------------------------------
def _safe_get(url, timeout=2.0):
    """
    Internal helper to safely handle GET requests, prevent freezes with timeouts,
    and avoid crashing on malformed or non-JSON HTTP error responses.
    """
    try:
        response = requests.get(url, timeout=timeout)
        # Raises an HTTPError if the status code was a 4xx or 5xx error
        response.raise_for_status() 
        return response.json()
    except requests.exceptions.HTTPError as e:
        # If Flask sent a custom error payload (like our 503 JSON messages)
        try:
            return response.json()
        except Exception:
            return {"status": "error", "message": f"Server error ({response.status_code}): {e}"}
    except requests.exceptions.RequestException as e:
        return {"status": "error", "message": f"Network transport connection failed: {e}"}

# ----------------------------------------------------
# Joystick initialization
# ----------------------------------------------------
def startJoystick():
    """Triggers the remote background device search and pairing engine loop."""
    url = f"{BASE_URL}/joystick/start"
    try:
        # Keeping your 45-second allowance for multi-sweep setup loops
        response = requests.post(url, timeout=45) 
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        try:
            return response.json() # Attempt to yield Flask's custom error JSON
        except Exception:
            return {"status": "error", "message": f"Connection to bridge failed: {e}"}

# ----------------------------------------------------
# Joystick metadata
# ----------------------------------------------------
def getName():
    """Queries the user-friendly hardware name string of the gamepad."""
    return _safe_get(f"{BASE_URL}/joystick/name")

def getNumAxes():
    """Queries total count of supported hardware continuous axes."""
    return _safe_get(f"{BASE_URL}/joystick/num_axes")

def getNumButtons():
    """Queries total count of supported binary hardware buttons."""
    return _safe_get(f"{BASE_URL}/joystick/num_buttons")

def getAxisMap():
    """Queries raw registered linux evdev integer codes for axes."""
    return _safe_get(f"{BASE_URL}/joystick/axis_map")

def getButtonMap():
    """Queries raw registered linux evdev integer codes for buttons."""
    return _safe_get(f"{BASE_URL}/joystick/button_map")

# ----------------------------------------------------
# Joystick live state (Optimized with short timeouts for hot-loops)
# ----------------------------------------------------
def getAxes():
    """Fetches full mapping dictionary of active axis tracking positions."""
    return _safe_get(f"{BASE_URL}/joystick/axes", timeout=0.5)

def getButtons():
    """Fetches full mapping dictionary of current button binary values."""
    return _safe_get(f"{BASE_URL}/joystick/buttons", timeout=0.5)

def getState():
    """
    Fetches full unified device map state packet. Consumes and resets 
    the change delta flags on the backend server.
    """
    return _safe_get(f"{BASE_URL}/joystick/state", timeout=0.5)

def getAxisRanges():
    """Queries full {axis_code: (min, max)} mapping from the backend."""
    return _safe_get(f"{BASE_URL}/joystick/axis_ranges")

def getAxisRange(code):
    """Queries the (min, max) range for a single axis code."""
    return _safe_get(f"{BASE_URL}/joystick/axis_range/{code}")
