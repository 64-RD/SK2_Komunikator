import PySimpleGUI as sg
import socket
from time import sleep


# Creates the window that lets user enter IP and port of the server
def get_ip():

    layout = [
        [sg.Text('Enter IP Address and Port.')],
        [sg.Text('IP:', size=(15, 1)), sg.InputText(), sg.Text('Port:', size=(5, 1)), sg.InputText()],
        [sg.Submit(), sg.Cancel()]
    ]
    window = sg.Window('SK2_Komunikator', layout)

    i = None
    p = None

    event, values = window.read()
    if event == 'Cancel' or event == sg.WIN_CLOSED:
        exit(0)
    else:
        window.close()
        i = values[0]
        p = int(values[1])
        return i, p


# Creates the window that handles the login phase
def handle_login(s):

    layout = [
        [sg.Text('Enter username and password.')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Text('Password:', size=(15, 1)), sg.InputText(password_char='*')],
        [sg.Submit(), sg.Cancel()]
    ]
    window = sg.Window('SK2_Komunikator', layout)

    while True:

        event, values = window.read()

        if event == 'Cancel' or event == sg.WIN_CLOSED:
            logout(s)
            exit(0)

        s.send(list(values.values())[0].encode())
        sleep(1)
        s.send(list(values.values())[1].encode())

        data = s.recv(1024).decode().strip()
        sg.popup(data)

        if data[6] == "s":
            window.close()
            return list(values.values())[0]


# Sends the logout request to server.
def logout(s):
    s.send("l".encode())
    return


# Sends the "get friends" request to server.
def get_friends(s):
    s.send("f".encode())
    data = s.recv(1024).decode()
    return data.replace('\x00', '').split('\n')


# Handles the incoming friend request.
def handle_friend(s, u):
    layout = [
        [sg.Text(f"User {u} would like to be your friend. Do you accept?")],
        [sg.Button("Yes"), sg.Button("No")]
    ]
    s.send('b'.encode())
    window = sg.Window('Friend Invite', layout)
    while True:
        event, _ = window.read()
        if event == 'Yes':
            s.send('t'.encode())
            break
        if event == 'No' or event == sg.WIN_CLOSED:
            s.send('f'.encode())
            break

    sleep(1)
    s.send(u.encode())
    window.close()
    return


# Sends the "get messages" request to server.
def get_msgs(s, u):
    s.send("g".encode())
    number = int.from_bytes(s.recv(4), "little")
    msgs = []
    for _ in range(number):
        data = s.recv(1024*8).decode().replace('\x00', '').split('\n')
        sender = data[0]
        content = data[1]
        if content == "":
            handle_friend(s, sender)
        else:
            msgs.append([sender, u, content])
    return msgs


# Opens the window that lets user add a friend.
def invite(s):

    layout = [
        [sg.Text('Add friend')],
        [sg.Text('Username:', size=(15, 1)), sg.InputText()],
        [sg.Button('Submit'), sg.Button('Cancel')]
    ]
    window = sg.Window('Adding friend', layout)
    username = ""
    while True:

        event, values = window.read()

        if event == 'Cancel' or event == sg.WIN_CLOSED:
            return

        if event == 'Submit':
            username = list(values.values())[0]
            window.close()
            break

    s.send("a".encode())
    sleep(1)
    s.send(username.encode())
    data = s.recv(1024).decode()
    sg.popup(data)
    return


# Sends the message to the server.
def send_msg(s, to, content):
    s.send("m".encode())
    sleep(1)
    s.send(to.encode())
    sleep(1)
    s.send(content.encode())


# Open the window that lets user remove a friend.
def delete_friend(s, friends):

    layout = [
        [sg.Text('Select friend to remove')],
        [sg.Listbox(values=friends, size=(20, 12), key='friends', enable_events=True)],
        [sg.Button('Submit'), sg.Button('Cancel')]
    ]
    window = sg.Window('Adding friend', layout)
    username = ""
    while True:

        event, values = window.read()

        if event == 'Cancel':
            return

        if event == 'Submit':
            username = values['friends'][0]
            window.close()
            break

    if not username:
        sg.popup("No friend selected...")
        return

    s.send("d".encode())
    sleep(1)
    s.send(username.encode())
    data = s.recv(1024).decode()
    sg.popup(data)
    return


# Updates current displayed text.
def update_text(msgs, curr):
    text = ""
    for msg in msgs:
        if msg[0] == curr or msg[1] == curr:
            text += f"{msg[0]}: {msg[2]}\n"
    return text


def main():

    # Get IP address and port and start connection
    while True:
        ip, port = get_ip()
        try:
            # Connection start attempt
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((ip, port))
            break

        except Exception as e:
            sg.popup(str(e))
            pass

    # Login phase
    username = handle_login(s)

    # Preparing the main window
    text = ""
    friends = get_friends(s)
    messages = get_msgs(s, username)
    layout = [
        [sg.Listbox(values=friends, size=(20, 12), key='friends', enable_events=True),
         sg.Column([[sg.Txt(size=(50,20), key='msgs', background_color='grey')],
                    [sg.Input(key='input', size=(20,5)), sg.Button('Send')]])],
        [sg.Button('Delete friend'), sg.Button('Add friend'), sg.Button('Logout')]
    ]

    # The main GUI part.
    window = sg.Window('SK2_Komunikator', layout)
    values = {'friends': []}
    last = ""
    counter = 0
    while True:

        # Window event handling
        event, values = window.read(timeout=500)

        # Exiting the program on button press
        if event == 'Logout' or event == sg.WIN_CLOSED:
            logout(s)
            break

        # Sending messages on button press
        if event == 'Send' and last != '':
            send_msg(s, last, values['input'])
            window['input'].update("")
            messages.append([username, last, values['input']])
            text = update_text(messages, last)

        # Adding friends on button press
        if event == 'Add friend':
            invite(s)

        # Adding friends on button press
        if event == 'Delete friend':
            delete_friend(s, friends)
            text = ""

        # A bit dumb checking if current friend was changed caused by API's construction
        try:
            last = values['friends'][0]
            text = update_text(messages, last)
        except IndexError:
            pass

        # Simple counter, so we don't flood server with update requests.
        if counter == 8:
            friends = get_friends(s)
            messages.extend(get_msgs(s, username))
            text = update_text(messages, last)
            counter = 0
        else:
            counter += 1

        # Update the window.
        window['friends'].update(friends)
        window['msgs'].update(text)

    # Close the window on program exit.
    window.close()


if __name__ == "__main__":
    main()
